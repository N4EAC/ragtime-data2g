// Past pictures in the chat history (app/src/Pictures): read and decoded on
// the reader's thread, their size known from their first bytes, and once a
// thumbnail is made the picture itself let go of, the thumbnail kept, and
// decoded again to view. This one needs a display for the window a
// thumbnail is sized for: without one it is skipped (exit 77).

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <string>
#include <thread>
#include <vector>

#include <wx/app.h>
#include <wx/frame.h>
#include <wx/init.h>

#include "Check.h"
#include "Pictures.h"
#include "ragtime/image/Codec.h"

namespace
{

class TestApp : public wxApp
{
public:
    bool OnInit() override { return true; }
};

std::vector<PictureReader::Done> waitFor(PictureReader& reader, size_t count)
{
    std::vector<PictureReader::Done> all;
    for (int i = 0; i < 500 && all.size() < count; i++)
    {
        for (PictureReader::Done& d : reader.take()) all.push_back(std::move(d));
        if (all.size() < count) std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    return all;
}

} // namespace

wxIMPLEMENT_APP_NO_MAIN(TestApp);

int main(int argc, char** argv)
{
    wxApp::SetInstance(new TestApp);
    if (!wxEntryStart(argc, argv) || !wxGetApp().CallOnInit())
    {
        std::printf("No display: skipped\n");
        return 77;
    }
    int result = 0;
    {
        const std::string tiny = std::string(RAGTIME_SOURCE_DIR) + "/tests/data/coolchic/tiny.rcc";
        std::ifstream in(tiny, std::ios::binary);
        std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        CHECK(!bytes.empty());

        // Its size from the header alone.
        int w = 0, h = 0;
        CHECK(pictureSize(bytes.data(), std::min<size_t>(bytes.size(), 512), w, h));
        CHECK(w > 0 && h > 0);
        int w2 = 0, h2 = 0;
        CHECK(!pictureSize(bytes.data(), 4, w2, h2));

        // Read on the thread: the picture, and a file that isn't one.
        PictureReader reader;
        reader.read(tiny);
        reader.read(tiny);      // asked twice, read once
        reader.read(std::string(RAGTIME_SOURCE_DIR) + "/tests/data/coolchic/no-such-file.rcc");
        std::vector<PictureReader::Done> done = waitFor(reader, 2);
        CHECK_EQ(done.size(), (size_t)2);
        CHECK(!reader.busy());
        const PictureReader::Done* picture = nullptr;
        for (const PictureReader::Done& d : done)
        {
            if (d.path == tiny) picture = &d;
            else CHECK(!d.ok);
        }
        CHECK(picture != nullptr);
        if (picture != nullptr && ragtime::image::available())
        {
            CHECK(picture->ok);
            CHECK_EQ(picture->image.width, w);
            CHECK_EQ(picture->image.height, h);
            CHECK_EQ(picture->size, (uint64_t)bytes.size());

            auto* frame = new wxFrame(nullptr, wxID_ANY, "Pictures");
            PictureShelf shelf;
            // A box its thumbnail's size while it is read.
            wxString blank = PictureShelf::blankHtml(w, h, frame, 240);
            CHECK(blank.Contains("<img"));

            shelf.shelve("pX", picture->image, picture->size);
            CHECK(shelf.settled("pX"));
            CHECK(shelf.find("pX") != nullptr);
            wxString thumb = shelf.thumbHtml("pX", frame, 240, "view");
            CHECK(thumb.Contains("memory:"));
            // The blank box and the thumbnail take the same room.
            int tw = 0, th = 0;
            ragtime::picture::fitWithin(w, h, frame->FromDIP(240), tw, th);
            const wxString room = wxString::Format("width=\"%d\" height=\"%d\"", tw, th);
            CHECK(blank.Contains(room));
            CHECK(thumb.Contains(room));

            // Let go of: the thumbnail stays, the picture goes.
            shelf.release("pX");
            CHECK(shelf.released("pX"));
            CHECK(shelf.settled("pX"));
            CHECK(shelf.find("pX") == nullptr);
            CHECK(shelf.thumbHtml("pX", frame, 240, "view") == thumb);
            // Shelved again to view: the picture back, the same thumbnail.
            shelf.shelve("pX", picture->image, picture->size);
            CHECK(!shelf.released("pX"));
            CHECK(shelf.find("pX") != nullptr);
            CHECK(shelf.thumbHtml("pX", frame, 240, "view") == thumb);
            frame->Destroy();
        }
        result = ragtime_test::finish();
    }
    wxEntryCleanup();
    return result;
}
