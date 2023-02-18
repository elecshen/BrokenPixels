#include <QList>
#include <iostream>
#include "tiffio.h"
#include <time.h>

using namespace std;

void calcAvg();

bool getImage(string path, uint32*& raster, uint32 &w, uint32 &h, size_t &npixels) {
    TIFF* tif = TIFFOpen(path.c_str(), "r");
    if(tif) {
        TIFFGetField(tif, TIFFTAG_IMAGEWIDTH, &w);
        TIFFGetField(tif, TIFFTAG_IMAGELENGTH, &h);
        npixels = w * h;
        raster = new uint32[npixels];

        if(raster != NULL) {
            if(TIFFReadRGBAImage(tif, w, h, raster, 0)) {
                return true;
            }
        }
    }
    return false;
}

QList<size_t>* avgBrokenPixelSearch(uint32* raster, uint32 w, size_t npixels, const double error) {
    /* Смещения индекса пикселя для выбора соседних пикселей
     * 0 - текущий пиксель
     * 5 6 7
     * 3   4
     * 0 1 2
     */
    const size_t adjacentPositions[] {0, 1, 2, w, w+2, w+w, w+w+1, w+w+2};
    const size_t comparedPosition = w+1;
    uint16 sum[4];
    bool isBrokenPixel;
    QList<size_t>* brokenPixels = new QList<size_t>;
    double delta;
    for(size_t i = 0; i < npixels-w-w-2; i++) {
        for(uint8 channel = 0; channel < 4; channel++)
            sum[channel] = 0;
        for(uint8 pos = 0; pos < size(adjacentPositions); pos++) {
            for(uint8 channel = 0; channel < 4; channel++) {
                sum[channel] += (raster[i + adjacentPositions[pos]] >> channel*8) & 0xff;
            }
        }
        isBrokenPixel = false;

        for(uint8 channel = 0; channel < 4; channel++) {
            delta = double(sum[channel])/8 - ((raster[i + comparedPosition] >> channel*8) & 0xff);
            if(delta >= 0 && delta > error) {
                isBrokenPixel = true;
                break;
            } else if(delta < 0 && delta < -error) {
                isBrokenPixel = true;
                break;
            }
        }
        if(isBrokenPixel)
            brokenPixels->append(i);
    }
    return brokenPixels;
}

// сравнить с записью сумм в массив

int main()
{
    time_t start, end;

    uint32* raster = nullptr; uint32 w = 0, h = 0; size_t npixels = 0;
    QList<size_t>* brokenPixels = nullptr;

    start = clock();

    if(getImage("D:\\source\\Qt\\untitled\\white_2.tif", raster, w, h, npixels)) {
        brokenPixels = avgBrokenPixelSearch(raster, w, npixels, 20);
    }
    end = clock();
    cout << "Milliseconds: " << end - start << endl;

    if(brokenPixels != nullptr)
        for(auto el: *brokenPixels){
            cout << el/w << " " << el%h << endl;
        }
}
