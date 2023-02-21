// This is a personal academic project. Dear PVS-Studio, please check it.

// PVS-Studio Static Code Analyzer for C, C++, C#, and Java: https://pvs-studio.com

#include <QSet>
#include <iostream>
#include <iomanip>
#include "tiffio.h"
#include <time.h>

using namespace std;

bool getImage(string path, uint32*& raster, uint32 &w, uint32 &h, size_t &npixels) {
    TIFF* tif = TIFFOpen(path.c_str(), "r");
    if(tif) {
        TIFFGetField(tif, TIFFTAG_IMAGEWIDTH, &w);
        TIFFGetField(tif, TIFFTAG_IMAGELENGTH, &h);
        npixels = w * h;
        raster = new uint32[npixels];

        if(raster != NULL) {
            if(TIFFReadRGBAImage(tif, w, h, raster, 0)) {
                TIFFClose(tif);
                return true;
            }
        }
    }
    TIFFClose(tif);
    return false;
}

QSet<size_t>* avgBrokenPixelSearch(uint32* raster, uint32 w, size_t npixels, const double error, uint8 k) {
    /* Смещения индекса пикселя для выбора соседних пикселей
     * 0 - текущий пиксель
     * 5 6 7
     * 3   4
     * 0 1 2
     *
     * 19 20 21 22 23
     * 14 15 16 17 18
     * 10 11    12 13
     *  5  6  7  8  9
     *  0  1  2  3  4
     */
    size_t* adjacentPositions;
    uint8 adjSize;
    size_t lineBorder;
    if(k == 3) {
        adjSize = 8;
        adjacentPositions = new size_t[adjSize]{0, 1, 2, w, w+2, 2*w, 2*w+1, 2*w+2};
        lineBorder = w-2;
    }
    else {
        adjSize = 24;
        adjacentPositions = new size_t[adjSize]{0, 1, 2, 3, 4, w, w+1, w+2, w+3, w+4, 2*w, 2*w+1, 2*w+3, 2*w+4, 3*w, 3*w+1, 3*w+2, 3*w+3, 3*w+4, 4*w, 4*w+1, 4*w+2, 4*w+3, 4*w+4};
        lineBorder = w-4;
    }
    const size_t comparedPosition = adjacentPositions[adjSize - 1] / 2;
    uint16 sum[4];
    bool isBrokenPixel;
    QSet<size_t>* brokenPixels = new QSet<size_t>;
    double delta;
    for(size_t i = 0; i < npixels-adjacentPositions[adjSize - 1]; i++) {
        if(!(i%w < lineBorder))
            continue;
        for(uint8 channel = 0; channel < 4; channel++)
            sum[channel] = 0;
        for(uint8 pos = 0; pos < adjSize; pos++) {
            for(uint8 channel = 0; channel < 4; channel++) {
                sum[channel] += (raster[i + adjacentPositions[pos]] >> channel*8) & 0xff;
            }
        }
        isBrokenPixel = false;

        for(uint8 channel = 0; channel < 4; channel++) {
            delta = double(sum[channel])/adjSize - ((raster[i + comparedPosition] >> channel*8) & 0xff);
            if(delta >= 0 && delta > error) {
                isBrokenPixel = true;
                break;
            } else if(delta < 0 && delta < -error) {
                isBrokenPixel = true;
                break;
            }
        }
        if(isBrokenPixel)
            *brokenPixels << i + comparedPosition;
    }
    delete[] adjacentPositions;
    return brokenPixels;
}

//QSet<size_t>* assocRulesBrokenPixelSearch(uint32* raster, uint32 w, size_t npixels, const double error) {
//    // p - позиция проверяемого пикселя, i - текущая позиция в массиве
//    // . . . 5 . . .
//    // . . . 4 . . .
//    // . . . 3 . . .
//    // 0 1 2 p 3 4 5
//    // . . . 2 . . .
//    // . . . 1 . . .
//    // i . . 0 . . .
//    const size_t horizontalPositions[] {w, w+1, w+2, w+4, w+5, w+6};
//    const size_t verticalPositions[] {3, w+3, 2*w+3, 3*w+3, 4*w+3, 5*w+3};


//}

uint8 median(uint8 f, uint8 s, uint8 t) {
    if(f < s){
        if(s < t) return s;
        if(f < t) return t;
        return f;
    }
    if(t < s) return s;
    if(f < t) return f;
    return t;
}

QSet<size_t>* medianBrokenPixelSearch(uint32* raster, uint32 w, size_t npixels, const double error) {
    const size_t poss[4][2] {{w, w+2}, {1, 2*w+1}, {0, 2*w+2}, {2*w, 2}};
    const size_t cPos = w+1;
    uint8 cPixel;
    uint16 channels[4];
    bool isBrokenPixel;
    QSet<size_t>* brokenPixels = new QSet<size_t>;
    int16 delta;

    for(size_t i = 0; i < npixels-w-w-2; i++) {
        if(!(i%w < w-2))
            continue;
        for(uint8 channel = 0; channel < 4; channel++) {
            cPixel = (raster[i + cPos] >> channel*8) & 0xff;
            channels[channel] = median(
                    cPixel,
                    median(
                        cPixel,
                        median( cPixel, ((raster[i + poss[0][0]] >> channel*8) & 0xff), ((raster[i + poss[0][1]] >> channel*8) & 0xff)),
                        median( cPixel, ((raster[i + poss[1][0]] >> channel*8) & 0xff), ((raster[i + poss[1][1]] >> channel*8) & 0xff))
                    ),
                    median(
                        cPixel,
                        median( cPixel, ((raster[i + poss[2][0]] >> channel*8) & 0xff), ((raster[i + poss[2][1]] >> channel*8) & 0xff)),
                        median( cPixel, ((raster[i + poss[3][0]] >> channel*8) & 0xff), ((raster[i + poss[3][1]] >> channel*8) & 0xff))
                    )
            );
        }

        isBrokenPixel = false;
        for(uint8 channel = 0; channel < 4; channel++) {
            delta = channels[channel] - ((raster[i + cPos] >> channel*8) & 0xff);
            if(delta >= 0 && delta > error) {
                isBrokenPixel = true;
                break;
            } else if(delta < 0 && delta < -error) {
                isBrokenPixel = true;
                break;
            }
        }
        if(isBrokenPixel)
            *brokenPixels << i + cPos;
    }
    return brokenPixels;
}

int main()
{
    time_t start, end;

    uint32* raster = nullptr; uint32 w = 0, h = 0; size_t npixels = 0;
    uint8 numberOfMethods = 3;
    QSet<size_t>** brokenPixels = new QSet<size_t>*[numberOfMethods];
    QSet<size_t> commonBrokenPixels;
    double error = 15;

    if(getImage("D:\\source\\Qt\\untitled\\white_2.tif", raster, w, h, npixels)) {
        start = clock();
        brokenPixels[0] = avgBrokenPixelSearch(raster, w, npixels, error, 3);
        end = clock();
        cout << "avg3 milliseconds: " << end - start << endl;

        start = clock();
        brokenPixels[1] = avgBrokenPixelSearch(raster, w, npixels, error, 5);
        end = clock();
        cout << "avg5 milliseconds: " << end - start << endl;

        start = clock();
        brokenPixels[2] = medianBrokenPixelSearch(raster, w, npixels, error);
        end = clock();
        cout << "median3 milliseconds: " << end - start << endl;
    }

    for(uint8 method = 0; method < numberOfMethods; method++) {
        commonBrokenPixels.unite(*(brokenPixels[method]));
    }
    QList<size_t> outputList(commonBrokenPixels.begin(), commonBrokenPixels.end());
    sort(outputList.begin(), outputList.end());

    cout << "Common pixels: " << outputList.count() << endl;
    cout << setw(11) << setfill(' ') << "";
    for(uint8 method = 0; method < numberOfMethods; method++) {
        cout << setw(9) << setfill(' ') << "Method " + to_string(method);
    }
    cout << endl;
    double counter;
    for(auto el: outputList){
        cout << setw(11) << setfill(' ') << "(" + to_string(el/w) + ";" + to_string(el%w) + ")";
        counter = 0;
        for(uint8 method = 0; method < numberOfMethods; method++) {
            if(brokenPixels[method]->contains(el)){
                counter++;
                cout << setw(9) << setfill(' ') << "True";
            }
            else {
                cout << setw(9) << setfill(' ') << "False";
            }
        }
        cout << "  " << counter/numberOfMethods*100 << "%" << endl;
    }

    delete[] raster;
    for(uint8 i = 0; i < numberOfMethods; i++)
        delete brokenPixels[i];
    delete[] brokenPixels;
}
