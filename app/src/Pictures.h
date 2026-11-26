#ifndef RAGTIME_PICTURES_H
#define RAGTIME_PICTURES_H

#include <condition_variable>
#include <cstdint>
#include <cstddef>
#include <deque>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <thread>
#include <vector>

#include <wx/gdicmn.h>
#include <wx/image.h>
#include <wx/string.h>

#include "ragtime/picture/Picture.h"

class wxWindow;

namespace ragtime
{
namespace image
{
namespace coolchic
{
class Decoder;
}
} // namespace image
} // namespace ragtime

// Inline pictures in the window: a
// picture read from disk in any format Ragtime knows, and the shelf of
// pictures shown among the messages, each decoded from its bytes as they
// stand and again only when more have come.

// Reads a picture to send: JPEG 2000, WebP and HEIF through the image
// library, everything else wxImage reads (JPEG, PNG, GIF, BMP, TIFF),
// turned the way up its EXIF says, transparency over white. False, with
// the reason, if it can't be.
bool loadPicture(const std::string& path, ragtime::picture::Image& out, std::string& error);

// The handlers wxImage reads pictures from disk with, each added once (wx
// keeps one of each). loadPicture adds them; call this on the window's
// thread before loadPicture runs on another.
void addPictureReadHandlers();

wxImage toWxImage(const ragtime::picture::Image& image);

// Decodes a picture's bytes, or as much of them as has come: a JPEG 2000
// codestream (kind 0x01) or a Cool-chic stream (kind 0x02,
// docs/PICTURE_CODEC.md), told apart by the codestream's first marker.
bool decodePicture(const uint8_t* bytes, size_t length, ragtime::picture::Image& out, std::string& error);

// A picture's own size from its first bytes, without decoding it: a JPEG
// 2000 codestream's SIZ, a Cool-chic stream's header. False if they don't
// say.
bool pictureSize(const uint8_t* bytes, size_t length, int& width, int& height);

// The file dialog's wildcard for pictures Ragtime reads.
wxString pictureWildcard();

class PictureShelf
{
public:
    // A picture as it stands: decoded from bytes of its size.
    struct Shelved
    {
        wxImage image;          // empty until something decodes
        uint64_t bytes = 0;     // decoded from this many
        uint64_t size = 0;      // of this many in all
        int width = 0;          // the codestream's own size
        int height = 0;
        int version = 0;        // moves each time it is decoded anew
    };

    // A picture's bytes as they stand (key: "b<id>" for a broadcast file,
    // "s<id>" for a session transfer, "p<path>" for one on disk): decoded
    // again if more have come, at most once a DECODE_MS unless it is whole
    // or final (it has ended, whole or not, and won't grow). True if what
    // shows changed.
    static constexpr uint64_t DECODE_MS = 1000;
    bool update(const std::string& key, const std::vector<uint8_t>& bytes, uint64_t size, uint64_t nowMs,
                bool final = false);
    // Whole or final, and shown: nothing more to read for it.
    bool settled(const std::string& key) const;
    // A picture decoded elsewhere (PictureReader), whole: shelved as
    // settled, its thumbnail kept if it has one.
    void shelve(const std::string& key, const ragtime::picture::Image& image, uint64_t size);
    // Lets go of the decoded picture once its thumbnail is made, keeping the
    // thumbnail (a past picture in the history: a thumbnail is a fraction
    // of the picture, and viewing it reads the file again). find() is then
    // null until it is shelved again.
    void release(const std::string& key);
    bool released(const std::string& key) const;
    const Shelved* find(const std::string& key) const;
    void forget(const std::string& key);

    // The picture among the messages: an <img> at most maxDip on its longer
    // side, linked to href; "" while nothing has decoded. With stopHref, a
    // button with the stop sign is drawn over its lower right corner, linked
    // to stopHref (one coming to us in a session, Jeff 2026-10-27).
    wxString thumbHtml(const std::string& key, const wxWindow* window, int maxDip, const wxString& href,
                       const wxString& stopHref = wxString());

    // An empty box the size a width x height picture's thumbnail takes,
    // for one still being read, so its row doesn't change height when the
    // picture comes.
    static wxString blankHtml(int width, int height, const wxWindow* window, int maxDip);

private:
    struct Entry
    {
        Shelved shelved;
        uint64_t decodedMs = 0;
        bool settled = false;
        std::string memoryName;     // its thumbnail in wxHTML's memory files
        int thumbVersion = -1;
        int thumbPx = 0;
        int thumbH = 0;
        bool released = false;      // the picture let go of, the thumbnail kept
        bool thumbStop = false;
        wxRect stopRect;            // the stop button's, in the thumbnail
        // A Cool-chic picture's decoder, fed more as it comes: each feed
        // decodes only the new latents.
        std::shared_ptr<ragtime::image::coolchic::Decoder> coolchic;
    };
    std::map<std::string, Entry> entries_;
    int nextName_ = 1;
};

// Past pictures (in the chat history) read from disk and decoded on a
// thread of its own, so scrolling through the history never waits on one:
// a 640 by 480 Cool-chic picture takes some 180 ms to decode. One file at a
// time, in the order asked; take() hands back those done.
class PictureReader
{
public:
    struct Done
    {
        std::string path;
        ragtime::picture::Image image;
        uint64_t size = 0;      // the file's bytes
        bool ok = false;        // false: unreadable, or not a picture
    };

    PictureReader() = default;
    ~PictureReader();
    PictureReader(const PictureReader&) = delete;
    PictureReader& operator=(const PictureReader&) = delete;

    // Queued, unless it is already waiting or being read.
    void read(const std::string& path);
    // Waiting, being read, or done and not yet taken.
    bool busy();
    std::vector<Done> take();

private:
    void run();

    std::mutex mutex_;
    std::condition_variable wake_;
    std::deque<std::string> queue_;
    std::set<std::string> asked_;   // queued or being read
    std::vector<Done> done_;
    bool stop_ = false;
    std::thread thread_;
};

// The stop button's side over a picture in a session's card.
constexpr int STOP_BUTTON_DIP = 40;

// The same button on its own, side DIPs square, as an <img> linked to
// href: a voice card's stop sign, beside its strip.
constexpr int VOICE_STOP_DIP = 28;
wxString stopSignHtml(const wxWindow* window, const wxString& href, int side = VOICE_STOP_DIP);


#endif
