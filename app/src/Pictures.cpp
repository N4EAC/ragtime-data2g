#include "Pictures.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iterator>
#include <memory>
#include <set>
#include <utility>

#include <wx/bitmap.h>
#include <wx/brush.h>
#include <wx/filesys.h>
#include <wx/font.h>
#include <wx/fs_mem.h>
#include <wx/graphics.h>
#include <wx/log.h>
#include <wx/mstream.h>
#include <wx/pen.h>
#include <wx/window.h>

#include "ragtime/files/SessionFiles.h"
#include "ragtime/image/Codec.h"
#include "ragtime/image/CoolChic.h"

using ragtime::picture::Image;

namespace
{

void addMemoryHandlers()
{
    // The seal badges and bells may have added them already: wx keeps one
    // of each.
    if (!wxFileSystem::HasHandlerForPath("memory:x")) wxFileSystem::AddHandler(new wxMemoryFSHandler);
    if (wxImage::FindHandler(wxBITMAP_TYPE_PNG) == nullptr) wxImage::AddHandler(new wxPNGHandler);
}

bool readAll(const std::string& path, std::vector<uint8_t>& out)
{
    std::ifstream in(path, std::ios::binary);
    if (!in) return false;
    out.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
    return !in.bad();
}

// A wxImage as an Image: RGB, any alpha laid over white.
Image fromWxImage(const wxImage& image)
{
    Image out = Image::blank(image.GetWidth(), image.GetHeight(), 3);
    const unsigned char* rgb = image.GetData();
    const unsigned char* alpha = image.HasAlpha() ? image.GetAlpha() : nullptr;
    const size_t n = (size_t)out.width * (size_t)out.height;
    for (size_t i = 0; i < n; i++)
    {
        for (int c = 0; c < 3; c++)
        {
            int v = rgb[i * 3 + c];
            if (alpha != nullptr) v = (v * alpha[i] + 255 * (255 - alpha[i]) + 127) / 255;
            out.pixels[i * 3 + c] = (uint8_t)v;
        }
    }
    return out;
}

} // namespace

void addPictureReadHandlers()
{
    if (wxImage::FindHandler(wxBITMAP_TYPE_JPEG) == nullptr) wxImage::AddHandler(new wxJPEGHandler);
    if (wxImage::FindHandler(wxBITMAP_TYPE_PNG) == nullptr) wxImage::AddHandler(new wxPNGHandler);
    if (wxImage::FindHandler(wxBITMAP_TYPE_GIF) == nullptr) wxImage::AddHandler(new wxGIFHandler);
    if (wxImage::FindHandler(wxBITMAP_TYPE_TIFF) == nullptr) wxImage::AddHandler(new wxTIFFHandler);
}

bool loadPicture(const std::string& path, Image& out, std::string& error)
{
    std::vector<uint8_t> bytes;
    if (!readAll(path, bytes) || bytes.empty())
    {
        error = "It can't be read.";
        return false;
    }
    ragtime::image::Load loaded = ragtime::image::load(bytes.data(), bytes.size(), out, error);
    if (loaded == ragtime::image::Load::Done) return true;
    if (loaded == ragtime::image::Load::Failed) return false;

    addPictureReadHandlers();
    wxMemoryInputStream stream(bytes.data(), bytes.size());
    wxImage image;
    {
        wxLogNull quiet; // wx's own complaint would pop up a box
        if (!image.LoadFile(stream, wxBITMAP_TYPE_ANY) || !image.IsOk())
        {
            error = "Ragtime doesn't know this kind of picture.";
            return false;
        }
    }
    if (image.HasMask() && !image.HasAlpha()) image.InitAlpha();
    out = ragtime::picture::orient(fromWxImage(image),
                                   ragtime::picture::jpegOrientation(bytes.data(), bytes.size()));
    return true;
}

wxImage toWxImage(const Image& image)
{
    if (image.empty()) return wxImage();
    wxImage out(image.width, image.height, false);
    unsigned char* rgb = out.GetData();
    const size_t n = (size_t)image.width * (size_t)image.height;
    for (size_t i = 0; i < n; i++)
    {
        for (int c = 0; c < 3; c++)
            rgb[i * 3 + c] = image.channels == 1 ? image.pixels[i] : image.pixels[i * image.channels + c];
    }
    return out;
}

bool decodePicture(const uint8_t* bytes, size_t length, Image& out, std::string& error)
{
    if (ragtime::picture::isCodestream(bytes, length)) return ragtime::image::decode(bytes, length, out, error);
    return ragtime::image::coolchicDecode(bytes, length, out, error);
}

bool pictureSize(const uint8_t* bytes, size_t length, int& width, int& height)
{
    width = height = 0;
    if (ragtime::picture::isCodestream(bytes, length))
    {
        int comps = 0;
        return ragtime::picture::codestreamSize(bytes, length, width, height, comps) && width > 0 && height > 0;
    }
    // Cool-chic's header is in its first few dozen bytes: fed only those,
    // the decoder reads the shape and decodes nothing.
    ragtime::image::coolchic::Decoder decoder;
    decoder.feed(bytes, std::min<size_t>(length, 256));
    const ragtime::image::coolchic::Architecture* arch = decoder.architecture();
    if (arch == nullptr || arch->width <= 0 || arch->height <= 0) return false;
    width = arch->width;
    height = arch->height;
    return true;
}

wxString pictureWildcard()
{
    wxString kinds = "*.jpg;*.jpeg;*.png;*.gif;*.bmp;*.tif;*.tiff;*.j2k;*.j2c;*.jpc;*.jp2";
    if (ragtime::image::available()) kinds += ";*.webp";
    if (ragtime::image::heifAvailable()) kinds += ";*.heic;*.heif;*.avif";
    // GTK matches the case given, and cameras shout.
    wxString both = kinds + ";" + kinds.Upper();
    return "Pictures (" + kinds + ")|" + both + "|All files (*)|*";
}

//-------------------------------------------------------------------------
// PictureShelf
//-------------------------------------------------------------------------

bool PictureShelf::update(const std::string& key, const std::vector<uint8_t>& bytes, uint64_t size,
                          uint64_t nowMs, bool final)
{
    Entry& e = entries_[key];
    Shelved& s = e.shelved;
    if (e.settled) return false;
    s.size = size;
    const bool whole = size > 0 && bytes.size() >= size;
    if (bytes.size() <= s.bytes)
    {
        e.settled = (final || whole) && s.version > 0;
        return false;
    }
    if (!whole && !final && s.version > 0 && nowMs < e.decodedMs + DECODE_MS) return false;

    Image image;
    std::string error;
    bool decoded = false;
    if (ragtime::picture::isCodestream(bytes.data(), bytes.size()))
    {
        if (s.width == 0)
        {
            int comps = 0;
            ragtime::picture::codestreamSize(bytes.data(), bytes.size(), s.width, s.height, comps);
        }
        decoded = ragtime::image::decode(bytes.data(), bytes.size(), image, error);
    }
    else
    {
        // Cool-chic: its decoder kept, fed the bytes so far.
        if (!e.coolchic) e.coolchic = std::make_shared<ragtime::image::coolchic::Decoder>();
        e.coolchic->feed(bytes.data(), bytes.size());
        if (const ragtime::image::coolchic::Architecture* arch = e.coolchic->architecture())
        {
            s.width = arch->width;
            s.height = arch->height;
        }
        decoded = e.coolchic->picture(image);
        if (whole || final) e.coolchic.reset();
    }
    if (!decoded)
    {
        // Not a picture after all, or not yet: an ended one isn't read again.
        e.settled = whole || final;
        e.decodedMs = nowMs;
        return false;
    }
    s.image = toWxImage(image);
    s.bytes = bytes.size();
    s.version++;
    e.decodedMs = nowMs;
    e.settled = whole || final;
    return true;
}

bool PictureShelf::settled(const std::string& key) const
{
    auto it = entries_.find(key);
    return it != entries_.end() && it->second.settled;
}

void PictureShelf::shelve(const std::string& key, const Image& image, uint64_t size)
{
    Entry& e = entries_[key];
    Shelved& s = e.shelved;
    s.image = toWxImage(image);
    s.width = image.width;
    s.height = image.height;
    s.bytes = s.size = size;
    e.coolchic.reset();
    e.settled = true;
    // Shelved again after a release: the thumbnail made before still holds.
    if (e.released) e.released = false;
    else s.version++;
}

void PictureShelf::release(const std::string& key)
{
    auto it = entries_.find(key);
    if (it == entries_.end()) return;
    Entry& e = it->second;
    if (!e.settled || e.memoryName.empty() || e.thumbStop || e.thumbVersion != e.shelved.version) return;
    e.shelved.image = wxImage();
    e.coolchic.reset();
    e.released = true;
}

bool PictureShelf::released(const std::string& key) const
{
    auto it = entries_.find(key);
    return it != entries_.end() && it->second.released;
}

const PictureShelf::Shelved* PictureShelf::find(const std::string& key) const
{
    auto it = entries_.find(key);
    if (it == entries_.end() || !it->second.shelved.image.IsOk()) return nullptr;
    return &it->second.shelved;
}

void PictureShelf::forget(const std::string& key)
{
    auto it = entries_.find(key);
    if (it == entries_.end()) return;
    if (!it->second.memoryName.empty()) wxMemoryFSHandler::RemoveFile(it->second.memoryName);
    entries_.erase(it);
}

namespace
{
// The stop sign in a button, drawn over a thumbnail's lower right corner.
wxRect drawStopButton(wxImage& thumb, const wxWindow* window);
} // namespace

wxString PictureShelf::thumbHtml(const std::string& key, const wxWindow* window, int maxDip, const wxString& href,
                                 const wxString& stopHref)
{
    auto it = entries_.find(key);
    if (it == entries_.end()) return wxString();
    Entry& e = it->second;
    const bool stop = !stopHref.empty();
    if (e.released)
    {
        // Only the thumbnail is kept: as it was made.
        if (stop || e.memoryName.empty()) return wxString();
        wxString img = wxString::Format("<img src=\"memory:%s\" width=\"%d\" height=\"%d\">",
                                        wxString(e.memoryName), e.thumbPx, e.thumbH);
        return href.empty() ? img : "<a href=\"" + href + "\">" + img + "</a>";
    }
    if (!e.shelved.image.IsOk()) return wxString();
    const wxImage& full = e.shelved.image;
    int w = 0, h = 0;
    ragtime::picture::fitWithin(full.GetWidth(), full.GetHeight(), window->FromDIP(maxDip), w, h);

    if (e.thumbVersion != e.shelved.version || e.thumbPx != w || e.thumbStop != stop)
    {
        addMemoryHandlers();
        if (!e.memoryName.empty()) wxMemoryFSHandler::RemoveFile(e.memoryName);
        // A new name each time, so wxHTML never shows one it cached.
        e.memoryName = wxString::Format("ragtime-picture-%d.png", nextName_++).ToStdString();
        wxImage thumb = full;
        if (w != full.GetWidth() || h != full.GetHeight()) thumb.Rescale(w, h, wxIMAGE_QUALITY_HIGH);
        e.stopRect = stop ? drawStopButton(thumb, window) : wxRect();
        wxMemoryFSHandler::AddFile(e.memoryName, thumb, wxBITMAP_TYPE_PNG);
        e.thumbVersion = e.shelved.version;
        e.thumbPx = w;
        e.thumbH = h;
        e.thumbStop = stop;
    }
    wxString img = wxString::Format("<img src=\"memory:%s\" width=\"%d\" height=\"%d\"", wxString(e.memoryName), w, h);
    if (stop && !e.stopRect.IsEmpty())
    {
        // An image map: the button's corner sends the stop sign, the rest
        // of the picture opens it, as the plain link does.
        wxString map = wxString::Format("ragtime-stop-map-%d", nextName_++);
        wxString html = img + " usemap=\"#" + map + "\">";
        html << "<map name=\"" << map << "\">"
             << wxString::Format("<area shape=\"rect\" coords=\"%d,%d,%d,%d\" href=\"", e.stopRect.GetLeft(),
                                 e.stopRect.GetTop(), e.stopRect.GetRight(), e.stopRect.GetBottom())
             << stopHref << "\">";
        if (!href.empty())
            html << wxString::Format("<area shape=\"rect\" coords=\"0,0,%d,%d\" href=\"", w - 1, h - 1) << href
                 << "\">";
        html << "</map>";
        return html;
    }
    img << ">";
    if (href.empty()) return img;
    return "<a href=\"" + href + "\">" + img + "</a>";
}

wxString PictureShelf::blankHtml(int width, int height, const wxWindow* window, int maxDip)
{
    int w = 0, h = 0;
    ragtime::picture::fitWithin(width, height, window->FromDIP(maxDip), w, h);
    if (w <= 0 || h <= 0) return wxString();
    addMemoryHandlers();
    // Clear, so it is only room: one file for each size, kept.
    static std::set<std::pair<int, int>> made;
    const wxString name = wxString::Format("ragtime-picture-blank-%dx%d.png", w, h);
    if (made.insert({w, h}).second)
    {
        wxImage blank(w, h);
        blank.InitAlpha();
        std::fill(blank.GetAlpha(), blank.GetAlpha() + (size_t)w * h, (unsigned char)0);
        wxMemoryFSHandler::AddFile(name, blank, wxBITMAP_TYPE_PNG);
    }
    return wxString::Format("<img src=\"memory:%s\" width=\"%d\" height=\"%d\">", name, w, h);
}

//-------------------------------------------------------------------------
// PictureReader
//-------------------------------------------------------------------------

PictureReader::~PictureReader()
{
    {
        std::lock_guard<std::mutex> lock(mutex_);
        stop_ = true;
    }
    wake_.notify_all();
    if (thread_.joinable()) thread_.join();
}

void PictureReader::read(const std::string& path)
{
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!asked_.insert(path).second) return;
        queue_.push_back(path);
        if (!thread_.joinable()) thread_ = std::thread([this]() { run(); });
    }
    wake_.notify_one();
}

bool PictureReader::busy()
{
    std::lock_guard<std::mutex> lock(mutex_);
    return !asked_.empty() || !done_.empty();
}

std::vector<PictureReader::Done> PictureReader::take()
{
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<Done> out;
    out.swap(done_);
    return out;
}

void PictureReader::run()
{
    while (true)
    {
        std::string path;
        {
            std::unique_lock<std::mutex> lock(mutex_);
            wake_.wait(lock, [this]() { return stop_ || !queue_.empty(); });
            if (stop_) return;
            path = queue_.front();
            queue_.pop_front();
        }
        Done done;
        done.path = path;
        std::ifstream in(ragtime::files::pathFromUtf8(path), std::ios::binary);
        std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        std::string error;
        done.size = bytes.size();
        done.ok = !bytes.empty() && decodePicture(bytes.data(), bytes.size(), done.image, error);
        std::lock_guard<std::mutex> lock(mutex_);
        asked_.erase(path);
        done_.push_back(std::move(done));
    }
}

//-------------------------------------------------------------------------
// The stop sign
//-------------------------------------------------------------------------

namespace
{

void drawStopSign(wxGraphicsContext& gc, double size)
{
    // An octagon, white-rimmed, with STOP across it.
    const double c = size / 2.0;
    const double pi = 3.14159265358979323846;
    auto octagon = [&](double r) {
        wxGraphicsPath path = gc.CreatePath();
        for (int i = 0; i < 8; i++)
        {
            const double a = pi / 8.0 + i * pi / 4.0;
            const double x = c + r * std::cos(a);
            const double y = c + r * std::sin(a);
            if (i == 0)
                path.MoveToPoint(x, y);
            else
                path.AddLineToPoint(x, y);
        }
        path.CloseSubpath();
        return path;
    };
    gc.SetPen(*wxTRANSPARENT_PEN);
    gc.SetBrush(wxBrush(wxColour(255, 255, 255)));
    gc.FillPath(octagon(size * 0.5));
    gc.SetBrush(wxBrush(wxColour(200, 16, 30)));
    gc.FillPath(octagon(size * 0.44));

    wxFont font(wxFontInfo(wxSize(0, std::max(4, (int)std::lround(size * 0.26)))).Bold());
    gc.SetFont(font, wxColour(255, 255, 255));
    double tw = 0, th = 0;
    gc.GetTextExtent("STOP", &tw, &th);
    if (tw > size * 0.8)
    {
        font.SetPixelSize(wxSize(0, std::max(4, (int)std::lround(size * 0.26 * size * 0.8 / tw))));
        gc.SetFont(font, wxColour(255, 255, 255));
        gc.GetTextExtent("STOP", &tw, &th);
    }
    gc.DrawText("STOP", c - tw / 2.0, c - th / 2.0);
}

wxRect drawStopButton(wxImage& thumb, const wxWindow* window)
{
    // A raised button, the stop sign inside, kept small on a small picture.
    const int w = thumb.GetWidth(), h = thumb.GetHeight();
    const int margin = window->FromDIP(6);
    const int side = std::min(window->FromDIP(STOP_BUTTON_DIP), std::min(w, h) / 2 - margin);
    if (side < window->FromDIP(16)) return wxRect();
    if (!thumb.HasAlpha()) thumb.InitAlpha();
    std::unique_ptr<wxGraphicsContext> gc(wxGraphicsContext::Create(thumb));
    if (!gc) return wxRect();
    gc->SetAntialiasMode(wxANTIALIAS_DEFAULT);
    const double x = w - margin - side, y = h - margin - side, r = side * 0.18;
    // Its shadow, its face and its rim.
    gc->SetPen(*wxTRANSPARENT_PEN);
    gc->SetBrush(wxBrush(wxColour(0, 0, 0, 90)));
    gc->DrawRoundedRectangle(x + 1.5, y + 2.0, side, side, r);
    gc->SetBrush(gc->CreateLinearGradientBrush(x, y, x, y + side, wxColour(252, 252, 252), wxColour(214, 214, 214)));
    gc->SetPen(wxPen(wxColour(110, 110, 110), std::max(1, window->FromDIP(1))));
    gc->DrawRoundedRectangle(x, y, side, side, r);
    // The sign, inset.
    const double inset = side * 0.12;
    gc->PushState();
    gc->Translate(x + inset, y + inset);
    drawStopSign(*gc, side - 2.0 * inset);
    gc->PopState();
    gc.reset();
    return wxRect((int)x, (int)y, side, side);
}

} // namespace

wxString stopSignHtml(const wxWindow* window, const wxString& href, int side)
{
    // One picture for each size asked, kept in wxHTML's memory files.
    static std::set<int> drawn;
    const int px = window->FromDIP(side);
    const wxString name = wxString::Format("ragtime-stop-sign-%d.png", px);
    if (drawn.count(px) == 0)
    {
        addMemoryHandlers();
        // drawStopButton draws inside a margin, at most half the picture:
        // a transparent picture twice the button's side, cut to it.
        const int margin = window->FromDIP(6);
        wxImage canvas(2 * (px + margin), 2 * (px + margin));
        canvas.InitAlpha();
        std::fill(canvas.GetAlpha(), canvas.GetAlpha() + canvas.GetWidth() * canvas.GetHeight(), (unsigned char)0);
        const wxRect r = drawStopButton(canvas, window);
        if (r.IsEmpty()) return wxString();
        // Room for the shadow below and right.
        const int extra = std::max(2, window->FromDIP(2));
        wxRect cut(r.x, r.y, std::min(r.width + extra, canvas.GetWidth() - r.x),
                   std::min(r.height + extra, canvas.GetHeight() - r.y));
        wxMemoryFSHandler::AddFile(name, canvas.GetSubImage(cut), wxBITMAP_TYPE_PNG);
        drawn.insert(px);
    }
    return "<a href=\"" + href + "\"><img src=\"memory:" + name + "\" align=\"middle\" alt=\"Stop: I have enough\"></a>";
}
