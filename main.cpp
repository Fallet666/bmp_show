#include <iostream>
#include <fstream>
#include <vector>
#include <cmath>
#include <stdexcept>
#include <future>
#include <thread>

#pragma pack(push, 1)

struct BMPFileHeader {
    uint16_t fileType{};
    uint32_t fileSize{};
    uint16_t reserved1{};
    uint16_t reserved2{};
    uint32_t offsetData{};
};

struct BMPInfoHeader {
    uint32_t size;
    int32_t width;
    int32_t height;
    uint16_t planes;
    uint16_t bitCount;
    uint32_t compression{};
    uint32_t imageSize{};
    int32_t xPixelsPerMeter{};
    int32_t yPixelsPerMeter{};
    uint32_t colorsUsed{};
    uint32_t colorsImportant{};
};

#pragma pack(pop)

class BMPImage {
private:
    BMPFileHeader fileHeader{};
    BMPInfoHeader infoHeader{};
    std::vector<uint8_t> pixelData{};
    int rowStride{0};
    constexpr static char WHITE = '#';
    constexpr static char BLACK = ' ';
    constexpr static uint8_t brightness_factor = 128;

    // Получение индекса пикселя
    [[nodiscard]] int getPixelIndex(int x, int y) const {
        return (y * rowStride) + (x * (infoHeader.bitCount / 8));
    }

public:
    void openBMP(const std::string &fileName) {
        std::ifstream file(fileName, std::ios::binary);
        if (!file) {
            throw std::runtime_error("Ошибка открытия файла: " + fileName);
        }

        // Чтение заголовков
        file.read(reinterpret_cast<char *>(&fileHeader), sizeof(fileHeader));
        if (file.gcount() != sizeof(fileHeader)) throw std::runtime_error("Ошибка чтения заголовка файла.");

        file.read(reinterpret_cast<char *>(&infoHeader), sizeof(infoHeader));
        if (file.gcount() != sizeof(infoHeader)) throw std::runtime_error("Ошибка чтения заголовка информации.");

        if (infoHeader.bitCount != 24 && infoHeader.bitCount != 32) {
            throw std::runtime_error("Неподдерживаемый формат BMP! Ожидалось 24 или 32 бита.");
        }

        file.seekg(fileHeader.offsetData, std::ios::beg);

        rowStride = (infoHeader.width * (infoHeader.bitCount / 8) + 3) & ~3;
        pixelData.resize(rowStride * infoHeader.height);
        file.read(reinterpret_cast<char *>(pixelData.data()), pixelData.size());
        if (file.gcount() != pixelData.size()) throw std::runtime_error("Ошибка чтения пикселей.");
    }

    [[nodiscard]] bool hasMoreThanTwoColors() const {
        for (int y = 0; y < infoHeader.height; ++y) {
            for (int x = 0; x < infoHeader.width; ++x) {
                int index = getPixelIndex(x, y);
                uint8_t blue = pixelData[index];
                uint8_t green = pixelData[index + 1];
                uint8_t red = pixelData[index + 2];
                if (!(red == 255 && green == 255 && blue == 255) && !(red == 0 && green == 0 && blue == 0))
                    return true;
            }
        }
        return false;
    }

    void convertToBlackAndWhite() {
        auto convertRow = [this](int startRow, int endRow, std::vector<uint8_t> &newPixelData) {
            for (int y = startRow; y < endRow; ++y) {
                for (int x = 0; x < infoHeader.width; ++x) {
                    int index = (y * rowStride) + (x * (infoHeader.bitCount / 8));

                    uint8_t blue = pixelData[index];
                    uint8_t green = pixelData[index + 1];
                    uint8_t red = pixelData[index + 2];

                    double brightness = 0.2126 * red + 0.7152 * green + 0.0722 * blue;

                    if (brightness < brightness_factor) {
                        newPixelData[index] = 0;
                        newPixelData[index + 1] = 0;
                        newPixelData[index + 2] = 0;
                    } else {
                        newPixelData[index] = 255;
                        newPixelData[index + 1] = 255;
                        newPixelData[index + 2] = 255;
                    }
                }
            }
        };

        std::vector<uint8_t> newPixelData = pixelData;

        // Получаем максимальное количество потоков
        unsigned int numThreads = std::thread::hardware_concurrency();
        int rowsPerThread = infoHeader.height / numThreads;
        std::vector<std::future<void> > futures;

        for (unsigned int i = 0; i < numThreads; ++i) {
            int startRow = i * rowsPerThread;
            int endRow = (i == numThreads - 1) ? infoHeader.height : startRow + rowsPerThread;
            // Последний поток берет оставшиеся строки

            futures.push_back(std::async(std::launch::async, convertRow, startRow, endRow, std::ref(newPixelData)));
        }

        for (auto &future: futures) {
            future.get();
        }

        pixelData = std::move(newPixelData);
    }

    void displayBMP() {
        if (hasMoreThanTwoColors()) {
            std::cout << "Изображение содержит более двух цветов, конвертируем в черно-белое..." << std::endl;
            convertToBlackAndWhite();
        }
        for (int y = infoHeader.height - 1; y >= 0; y -= 2) {
            for (int x = 0; x < infoHeader.width; ++x) {
                int index = getPixelIndex(x, y);
                uint8_t blue = pixelData[index];
                uint8_t green = pixelData[index + 1];
                uint8_t red = pixelData[index + 2];

                std::cout << ((red == 255 && green == 255 && blue == 255) ? WHITE : BLACK);
            }
            std::cout << std::endl;
        }
    }

    void clearData() {
        pixelData.clear();
    }
};

int main(int argc, char *argv[]) {
    try {
        if (argc != 2) {
            throw std::runtime_error("Использование: <путь_к_файлу.bmp>");
        }

        BMPImage image;
        image.openBMP(argv[1]);
        image.displayBMP();
        image.clearData();
    } catch (const std::exception &e) {
        std::cerr << "Ошибка: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
