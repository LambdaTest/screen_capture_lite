#include "ScreenCapture.h"
#include "internal/SCCommon.h" //DONT USE THIS HEADER IN PRODUCTION CODE!!!! ITS INTERNAL FOR A REASON IT WILL CHANGE!!! ITS HERE FOR TESTS ONLY!!!
#include <algorithm>
#include <atomic>
#include <chrono>
#include <climits>
#include <iostream>
#include <locale>
#include <string>
#include <thread>
#include <vector>

// THESE LIBRARIES ARE HERE FOR CONVINIENCE!! They are SLOW and ONLY USED FOR
// HOW THE LIBRARY WORKS!
#define TJE_IMPLEMENTATION
#include "tiny_jpeg.h"
#define LODEPNG_COMPILE_PNG
#define LODEPNG_COMPILE_DISK
#include "lodepng.h"
/////////////////////////////////////////////////////////////////////////

void ExtractAndConvertToRGBA(const SL::Screen_Capture::Image &img, unsigned char *dst, size_t dst_size)
{
    assert(dst_size >= static_cast<size_t>(SL::Screen_Capture::Width(img) * SL::Screen_Capture::Height(img) * sizeof(SL::Screen_Capture::ImageBGRA)));
    auto imgsrc = StartSrc(img);
    auto imgdist = dst;
    if (img.isContiguous) {
        memcpy(imgdist, imgsrc, dst_size);
    }
    else { 
        for (auto h = 0; h < Height(img); h++) {
            auto startimgsrc = imgsrc;
            for (auto w = 0; w < Width(img); w++) {
                *imgdist++ = imgsrc->R;
                *imgdist++ = imgsrc->G;
                *imgdist++ = imgsrc->B;
                *imgdist++ = 0; // alpha should be zero
                imgsrc++;
            }
            imgsrc = SL::Screen_Capture::GotoNextRow(img, startimgsrc);
        }
    }
}

static const std::string BASE64 = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

void toBase64(std::string &out, std::vector<unsigned char> &in)
{
    for (size_t i = 0; i < in.size(); i += 3) {
        int v = 65536 * in[i];
        if (i + 1 < in.size())
            v += 256 * in[i + 1];
        if (i + 2 < in.size())
            v += in[i + 2];
        out.push_back(BASE64[(v >> 18) & 0x3f]);
        out.push_back(BASE64[(v >> 12) & 0x3f]);
        if (i + 1 < in.size())
            out.push_back(BASE64[(v >> 6) & 0x3f]);
        else
            out.push_back('=');
        if (i + 2 < in.size())
            out.push_back(BASE64[(v >> 0) & 0x3f]);
        else
            out.push_back('=');
    }
}

using namespace std::chrono_literals;
std::shared_ptr<SL::Screen_Capture::IScreenCaptureManager> framgrabber;
std::atomic<int> realcounter;
std::atomic<int> onNewFramecounter;

inline std::ostream &operator<<(std::ostream &os, const SL::Screen_Capture::ImageRect &p)
{
    return os << "left=" << p.left << " top=" << p.top << " right=" << p.right << " bottom=" << p.bottom;
}
inline std::ostream &operator<<(std::ostream &os, const SL::Screen_Capture::Monitor &p)
{
    return os << "Id=" << p.Id << " Index=" << p.Index << " Height=" << p.Height << " Width=" << p.Width << " OffsetX=" << p.OffsetX
              << " OffsetY=" << p.OffsetY << " Name=" << p.Name;
}

auto onNewFramestart = std::chrono::high_resolution_clock::now();
void createframegrabber()
{
    realcounter = 0;
    onNewFramecounter = 0;
    framgrabber = nullptr;
    framgrabber =
        SL::Screen_Capture::CreateCaptureConfiguration([]() {
            auto mons = SL::Screen_Capture::GetMonitors();
            //std::cout << "Library is requesting the list of monitors to capture!" << std::endl;
            for (auto &m : mons) {
                //std::cout << m << std::endl;
            }
            return mons;
        })
            ->onFrameChanged([&](const SL::Screen_Capture::Image &img, const SL::Screen_Capture::Monitor &monitor) {
                //std::cout << "Difference detected!  " << img.Bounds << std::endl;
                // Uncomment the below code to write the image to disk for debugging
                /*
                        auto r = realcounter.fetch_add(1);
                        auto s = std::to_string(r) + std::string("MONITORDIF_") + std::string(".jpg");
                        auto size = Width(img) * Height(img) * sizeof(SL::Screen_Capture::ImageBGRA);
                         auto imgbuffer(std::make_unique<unsigned char[]>(size));
                         ExtractAndConvertToRGBA(img, imgbuffer.get(), size);
                         tje_encode_to_file(s.c_str(), Width(img), Height(img), 4, (const unsigned char*)imgbuffer.get());
                */
            })
            ->onNewFrame([&](const SL::Screen_Capture::Image &img, const SL::Screen_Capture::Monitor &monitor) {
                // Uncomment the below code to write the image to disk for debugging
                /*
                        auto r = realcounter.fetch_add(1);
                        auto s = std::to_string(r) + std::string("MONITORNEW_") + std::string(".jpg");
                        auto size = Width(img) * Height(img) * sizeof(SL::Screen_Capture::ImageBGRA);
                        auto imgbuffer(std::make_unique<unsigned char[]>(size));
                        ExtractAndConvertToRGBA(img, imgbuffer.get(), size);
                        tje_encode_to_file(s.c_str(), Width(img), Height(img), 4, (const unsigned char *)imgbuffer.get());
                        tje_encode_to_file(s.c_str(), Width(img), Height(img), 4, (const unsigned char *)imgbuffer.get());
                */
                if (std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - onNewFramestart).count() >=
                    1000) {
                    //std::cout << "onNewFrame fps" << onNewFramecounter << std::endl;
                    onNewFramecounter = 0;
                    onNewFramestart = std::chrono::high_resolution_clock::now();
                }
                onNewFramecounter += 1;
            })
            ->onMouseChanged([&](const SL::Screen_Capture::Image *img, const SL::Screen_Capture::MousePoint &mousepoint) {
                // Uncomment the below code to write the image to disk for debugging
               // auto r = realcounter.fetch_add(1);
                //auto s = std::to_string(r) + std::string(" M") + std::string(".png");
                std::vector<unsigned char> input_vector_img;
                std::string base64_output;
                if (img) {
                    //auto size = Width(*img) * Height(*img) * sizeof(SL::Screen_Capture::ImageBGRA);
                    lodepng::encode(input_vector_img, (unsigned char *)StartSrc(*img), Width(*img), Height(*img));
                    toBase64(base64_output,input_vector_img);
                    std::cout << base64_output;
                }
            })
            ->start_capturing();

    framgrabber->setFrameChangeInterval(std::chrono::milliseconds(100));
    framgrabber->setMouseChangeInterval(std::chrono::milliseconds(100));
}




int main()
{
    std::srand(std::time(nullptr));
    //std::cout << "Starting Capture Demo/Test" << std::endl;
    //std::cout << "Testing captured monitor bounds check" << std::endl;

    auto goodmonitors = SL::Screen_Capture::GetMonitors();
    for (auto &m : goodmonitors) {
       // std::cout << m << std::endl;
        assert(SL::Screen_Capture::isMonitorInsideBounds(goodmonitors, m));
    }
    auto badmonitors = SL::Screen_Capture::GetMonitors();

    for (auto m : badmonitors) {
        m.Height += 1;
        //std::cout << m << std::endl;
        assert(!SL::Screen_Capture::isMonitorInsideBounds(goodmonitors, m));
    }
    for (auto m : badmonitors) {
        m.Width += 1;
        //std::cout << m << std::endl;
        assert(!SL::Screen_Capture::isMonitorInsideBounds(goodmonitors, m));
    }
    //std::cout << "Running display capturing for 10 seconds" << std::endl;
    createframegrabber();
    std::this_thread::sleep_for(std::chrono::seconds(10));


    //std::cout << "Pausing for 10 seconds. " << std::endl;
    framgrabber->pause();
    auto counti = 0;
    while (counti++ < 10) {
        assert(framgrabber->isPaused());
        //std::cout << " . ";
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    //std::cout << std::endl << "Resuming  . . . for 5 seconds" << std::endl;
    framgrabber->resume();
    //std::this_thread::sleep_for(std::chrono::seconds(5));

    //std::cout << "Testing changing the interval during runtime for race conditions " << std::endl;

    // HAMMER THE SET FRAME INTERVAL FOR RACE CONDITIONS!!
    auto start = std::chrono::high_resolution_clock::now();
    //while (std::chrono::duration_cast<std::chrono::seconds>(std::chrono::high_resolution_clock::now() - start).count() < 10) {
    while (true){
        for (auto t = 0; t < 100; t++) {
            framgrabber->setFrameChangeInterval(std::chrono::microseconds(100));
            framgrabber->setMouseChangeInterval(std::chrono::microseconds(100));
        }
    }

    //std::cout << "Changing the cpature rate to 1 second" << std::endl;
    framgrabber->setFrameChangeInterval(std::chrono::seconds(1));
    std::this_thread::sleep_for(std::chrono::seconds(5));

    //std::cout << "Setting timer using chrono literals" << std::endl;
    // You can use chron's literals as well!
    framgrabber->setFrameChangeInterval(10ms);
    std::this_thread::sleep_for(std::chrono::seconds(5));

    //std::cout << "Testing recreating" << std::endl;
    createframegrabber();
    std::this_thread::sleep_for(std::chrono::seconds(5));

    //std::cout << "Testing destroy" << std::endl;
    framgrabber = nullptr;

    //std::cout << "Testing recreating" << std::endl;
    createframegrabber();
    std::this_thread::sleep_for(std::chrono::seconds(5));

    // 4k image
    int height = 2160;
    int width = 3840;
    std::vector<SL::Screen_Capture::ImageBGRA> image1, image2;
    image1.resize(height * width);
    for (auto &a : image1) {
        a.B = static_cast<unsigned short>(std::rand() % 255);
        a.A = static_cast<unsigned short>(std::rand() % 255);
        a.G = static_cast<unsigned short>(std::rand() % 255);
        a.R = static_cast<unsigned short>(std::rand() % 255);
    }
    image2.resize(height * width);
    for (auto &a : image2) {
        a.B = static_cast<unsigned short>(std::rand() % 255);
        a.A = static_cast<unsigned short>(std::rand() % 255);
        a.G = static_cast<unsigned short>(std::rand() % 255);
        a.R = static_cast<unsigned short>(std::rand() % 255);
    }
    long long durationaverage = 0;
    long long smallestduration = INT_MAX;
    for (auto i = 0; i < 100; i++) { // run a few times to get an average
        auto starttime = std::chrono::high_resolution_clock::now();
        auto difs =
            SL::Screen_Capture::GetDifs(SL::Screen_Capture::CreateImage(SL::Screen_Capture::ImageRect(0, 0, width, height), 0, image1.data()),
                                        SL::Screen_Capture::CreateImage(SL::Screen_Capture::ImageRect(0, 0, width, height), 0, image2.data()));
        long long d = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now() - starttime).count();
        smallestduration = std::min(d, smallestduration);
        durationaverage += std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now() - starttime).count();
    }
    durationaverage /= 100;
    //std::cout << "Best Case -- Time to get diffs " << durationaverage << " microseconds" << std::endl;
    //std::cout << "Best Case -- Lowest Time " << smallestduration << " microseconds" << std::endl;
    memset(image1.data(), 5, image1.size() * sizeof(SL::Screen_Capture::ImageBGRA));
    memset(image2.data(), 5, image2.size() * sizeof(SL::Screen_Capture::ImageBGRA));

    durationaverage = 0;
    smallestduration = INT_MAX;
    for (auto i = 0; i < 100; i++) { // run a few times to get an average
        auto starttime = std::chrono::high_resolution_clock::now();
        auto difs =
            SL::Screen_Capture::GetDifs(SL::Screen_Capture::CreateImage(SL::Screen_Capture::ImageRect(0, 0, width, height), 0, image1.data()),
                                        SL::Screen_Capture::CreateImage(SL::Screen_Capture::ImageRect(0, 0, width, height), 0, image2.data()));
        long long d = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now() - starttime).count();
        smallestduration = std::min(d, smallestduration);
        durationaverage += std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now() - starttime).count();
    }
    durationaverage /= 100;
    //std::cout << "Worst Case -- Time to get diffs " << durationaverage << " microseconds" << std::endl;
    //std::cout << "Worst Case -- Lowest Time " << smallestduration << " microseconds" << std::endl;

    return 0;
}
