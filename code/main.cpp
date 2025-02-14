#include <iostream>
#include <fstream>
#include <vector>
#include <cstdint>
#include <string>
#include <filesystem>
#include <windows.h>

// BMP file header structure
#pragma pack(push, 1)
struct BMPFileHeader {
    uint16_t fileType{ 0x4D42 };     // File type always BM (0x4D42)
    uint32_t fileSize{ 0 };           // Size of the file in bytes
    uint16_t reserved1{ 0 };          // Reserved, must be 0
    uint16_t reserved2{ 0 };          // Reserved, must be 0
    uint32_t offsetData{ 0 };         // Start position of pixel data (bytes from the beginning of the file)
};

// BMP info header structure (for 24-bit BMP)
struct BMPInfoHeader {
    uint32_t size{ 0 };               // Size of this header (40 bytes)
    int32_t width{ 0 };               // Width of the bitmap in pixels
    int32_t height{ 0 };              // Height of the bitmap in pixels
    uint16_t planes{ 1 };             // Number of color planes, must be 1
    uint16_t bitCount{ 0 };           // Number of bits per pixel (24 for 24-bit bitmap)
    uint32_t compression{ 0 };        // Compression type (0 for no compression)
    uint32_t sizeImage{ 0 };          // Size of the raw bitmap data
    int32_t xPixelsPerMeter{ 0 };     // Horizontal resolution (pixels per meter)
    int32_t yPixelsPerMeter{ 0 };     // Vertical resolution (pixels per meter)
    uint32_t colorsUsed{ 0 };         // Number of colors in the color palette
    uint32_t colorsImportant{ 0 };    // Important colors (generally ignored)
};


// Pixel structure (BGR format)
struct BMPColor {
    uint8_t blue;
    uint8_t green;
    uint8_t red;
    uint8_t alpha = 255;
};
#pragma pack(pop)

// BMP image class to hold image data
class BMPImage {
public:
    BMPImage() = default;
    bool load(const std::string& filename);
    void printInfo() const;
    const std::vector<BMPColor>& getPixels() const { return pixels; }
    const int getWidth() const { return infoHeader.width; }
    const int getHeight() const { return infoHeader.height; }

private:
    std::string filename;
    BMPFileHeader fileHeader;
    BMPInfoHeader infoHeader;
    std::vector<BMPColor> pixels;
};

bool BMPImage::load(const std::string& filename)
{
    std::ifstream file(filename, std::ios::binary);
    if (!file) {
        std::cerr << "Unable to open file " << filename << "\n";
        return false;
    }

    // Read file header
    file.read(reinterpret_cast<char*>(&fileHeader), sizeof(fileHeader));
    if (fileHeader.fileType != 0x4D42) {
        std::cerr << "Not a BMP file\n";
        return false;
    }

    // Read info header
    file.read(reinterpret_cast<char*>(&infoHeader), sizeof(infoHeader));

    // Ensure it's a 24-bit uncompressed BMP
    if (infoHeader.bitCount != 24 || infoHeader.compression != 0) {
        std::cerr << "Unsupported BMP format (must be 24-bit, uncompressed)\n";
        return false;
    }

    // Move to the start of pixel data
    file.seekg(fileHeader.offsetData, std::ios::beg);

    // Resize pixel vector to hold the image data
    pixels.resize(infoHeader.width * std::abs(infoHeader.height));

    // Calculate padding (each row in BMP is padded to be a multiple of 4 bytes)
    int rowSize = ((infoHeader.width * 3 + 3) & (~3));
    int padding = rowSize - (infoHeader.width * 3);

    // Temporary buffer to read each row of 24-bit data
    std::vector<uint8_t> row(rowSize);

    // Read pixel data and convert to BGRA
    for (int y = std::abs(infoHeader.height) - 1; y >= 0; --y) {
        file.read(reinterpret_cast<char*>(row.data()), rowSize);
        for (int x = 0; x < infoHeader.width; ++x) {
            BMPColor color;
            color.blue = row[x * 3];
            color.green = row[x * 3 + 1];
            color.red = row[x * 3 + 2];
            color.alpha = 255; // Full opacity
            pixels[y * infoHeader.width + x] = color;
        }
    }

    file.close();
    return true;
}

void BMPImage::printInfo() const {
    std::cout << "Width: " << infoHeader.width << "\n";
    std::cout << "Height: " << infoHeader.height << "\n";
    std::cout << "Bit Depth: " << infoHeader.bitCount << "\n";
}

// Function to get all BMP file paths in a directory
std::vector<std::string> getBMPFiles(const std::string& directory) {
    std::vector<std::string> bmpFiles;
    for (const auto& entry : std::filesystem::directory_iterator(directory)) {
        if (entry.path().extension() == ".bmp") {
            bmpFiles.push_back(entry.path().string());
        }
    }
    return bmpFiles;
}

// Globals to keep track of images and current index
std::vector<std::string> bmpFiles;
int currentImageIndex = 0;
BMPImage image;
HDC hdcMem = nullptr;
HBITMAP hBitmap = nullptr;

// Load the image at the current index
bool loadCurrentImage(HWND hwnd) {
    if (hdcMem) DeleteDC(hdcMem);
    if (hBitmap) DeleteObject(hBitmap);

    if (!image.load(bmpFiles[currentImageIndex])) {
        MessageBox(hwnd, "Failed to load BMP file", "Error", MB_OK | MB_ICONERROR);
        return false;
    }

    // Get the image dimensions
    int imageWidth = image.getWidth();
    int imageHeight = image.getHeight();

    // Adjust window size to fit the image
    RECT rect = { 0, 0, imageWidth, imageHeight };
    AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, FALSE);

    SetWindowPos(hwnd, nullptr, 0, 0, rect.right - rect.left, rect.bottom - rect.top,
        SWP_NOMOVE | SWP_NOZORDER);

    HDC hdc = GetDC(hwnd);
    hdcMem = CreateCompatibleDC(hdc);

    BITMAPINFO bmi = {};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = image.getWidth();
    bmi.bmiHeader.biHeight = -image.getHeight();
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32; // 32-bit color
    bmi.bmiHeader.biCompression = BI_RGB;

    void* bitmapData = nullptr;
    hBitmap = CreateDIBSection(hdc, &bmi, DIB_RGB_COLORS, &bitmapData, nullptr, 0);

    if (bitmapData) {
        memcpy(bitmapData, image.getPixels().data(), image.getPixels().size() * sizeof(BMPColor));
    }

    SelectObject(hdcMem, hBitmap);
    ReleaseDC(hwnd, hdc);
    InvalidateRect(hwnd, nullptr, TRUE);  // Request a repaint

    return true;
}

// Window procedure to handle events
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
    case WM_CREATE: {
        bmpFiles = getBMPFiles(".\\");
        if (bmpFiles.empty()) {
            MessageBox(hwnd, "No BMP files found in the 'images' folder", "Error", MB_OK | MB_ICONERROR);
            PostQuitMessage(0);
            return 0;
        }
        currentImageIndex = 0;
        if (!loadCurrentImage(hwnd)) {
            PostQuitMessage(0);
        }
    } break;
    case WM_KEYDOWN: {
        if (wParam == VK_RIGHT) { // Right arrow key
            currentImageIndex = (currentImageIndex + 1) % bmpFiles.size();
            loadCurrentImage(hwnd);
        }
        else if (wParam == VK_LEFT) { // Left arrow key
            currentImageIndex = (currentImageIndex - 1 + bmpFiles.size()) % bmpFiles.size();
            loadCurrentImage(hwnd);
        }
    } break;

    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);

        // Clear the window with a white background
        RECT rect;
        GetClientRect(hwnd, &rect);
        FillRect(hdc, &rect, (HBRUSH)(COLOR_WINDOW + 1));

        BitBlt(hdc, 0, 0, image.getWidth(), image.getHeight(), hdcMem, 0, 0, SRCCOPY);
        EndPaint(hwnd, &ps);
    } break;

    case WM_DESTROY: {
        if (hBitmap) DeleteObject(hBitmap);
        if (hdcMem) DeleteDC(hdcMem);
        PostQuitMessage(0);
    } break;

    default:
        return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }
    return 0;
}

// Win32 entry point
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow) {
    const char CLASS_NAME[] = "BMPViewer";

    WNDCLASS wc = {};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;

    RegisterClass(&wc);

    HWND hwnd = CreateWindowEx(0, CLASS_NAME, "BMP Viewer", WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 800, 600,
        nullptr, nullptr, hInstance, nullptr);

    if (!hwnd) return 0;

    ShowWindow(hwnd, nCmdShow);

    MSG msg = {};
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return 0;
}
