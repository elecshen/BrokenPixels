#include <QSet>
#include <iostream>
#include <iomanip>
#include "tiffio.h"
#include <time.h>

using namespace std;

/*!
 * \brief Чтения файла изображения и получение массива пикселей и размеров изображения(изображения читается с левого нижнего угла)
 * \param path - Путь до изображени
 * \param raster - Массив в который будут записаны начения пикселей
 * \param w - Ширина изображения
 * \param h - Высота изображения
 * \param npixels - Количество пикселей
 * \return В случае успеха вернёт 0, иначе вернёт код ошибки.
 * Коды ошибок:
 * 1 - не удалось открыть изображение
 * 2 - конфигурация изображения не поддерживается
 * 3 - изображение слишком маленькое
 * 4 - не удалось выделить память
 * 5 - не удалось прочитать изображение
 */
uint8 getImage(char* path, uint16*& raster, uint32 &w, uint32 &h, size_t &npixels) {
    uint8 errCode = 0;
    TIFF* tif = TIFFOpen(path, "r");
    if(tif) {
        uint16 t;
        TIFFGetField(tif, TIFFTAG_BITSPERSAMPLE, &t);
        if(t != 16) errCode = 2;
        TIFFGetField(tif, TIFFTAG_SAMPLESPERPIXEL, &t);
        if(t != 1) errCode = 2;
        TIFFGetField(tif, TIFFTAG_PHOTOMETRIC, &t);
        if(t != PHOTOMETRIC_MINISBLACK) errCode = 2;
        TIFFGetField(tif, TIFFTAG_PLANARCONFIG, &t);
        if(t != PLANARCONFIG_CONTIG) errCode = 2;
        if(errCode != 0) {
            TIFFClose(tif);
            return errCode;
        }

        TIFFGetField(tif, TIFFTAG_IMAGEWIDTH, &w);
        if(w < 5) errCode = 3;
        TIFFGetField(tif, TIFFTAG_IMAGELENGTH, &h);
        if(h < 5) errCode = 3;
        if(errCode != 0) {
            TIFFClose(tif);
            return errCode;
        }

        npixels = w * h;
        raster = new uint16[npixels];
        if(raster != NULL) {
            for(size_t row = 0; row < h; row++) {
                if(TIFFReadScanline(tif, raster+row*w, row) == -1) {
                    errCode = 5;
                    break;
                }
            }
        }
        else errCode = 4;
    }
    else errCode = 1;
    TIFFClose(tif);
    return errCode;
}

/*!
 * \brief Проверка того, что разница пикселей превысит заданный порог
 * \param delta - разница значений цветов пикселей
 * \param threshold - пороговое значение
 * \return Если порог превышен, то возвращает true, иначе false
 */
bool isExceedThreshold(int32 delta, const uint16 threshold) {
    if(delta >= 0 && delta > threshold) {
        return true;
    } else if(delta < 0 && delta < -threshold) {
        return true;
    }
    return false;
}

/*!
 * \brief Поиск битых пикселей посредством сравнения их со средним значением окружающих пикселей в квадрате k*k
 * Доступные значения:
 * k = 3 - высчитываются значения пикселей в квадрате 3х3 без учёта центрального(проверяемого)
 * k = 5 - вычисление для квадрата 5*5
 * \param raster - массив пикселей
 * \param w - ширина изображения
 * \param npixels - количество пикселей
 * \param threshold - порог по которому будут отбираться искомые пиксели
 * \param k - размер поля для вычисления среднего значения
 * \return Возвращается коллекция индексов пикселей, отобранных алгоритмом. Вслучае неверного значения параметра k возращает nullptr.
 */
QSet<size_t>* avgBrokenPixelSearch(uint16* raster, uint32 w, size_t npixels, const uint16 threshold, uint8 k) {
    /* Смещения индекса пикселя для выбора соседних пикселей
     * 0 - текущий пиксель (на индекс которого указывает переменная итератор)
     * p - пиксель, со значением которого будет производится сравнение
     * 5 6 7
     * 3 p 4
     * 0 1 2
     *
     * 19 20 21 22 23
     * 14 15 16 17 18
     * 10 11  p 12 13
     *  5  6  7  8  9
     *  0  1  2  3  4
     */
    size_t* adjacentPositions; // Массив, где будут храниться смещения (на сколько нужно сместить индекс)
    uint8 adjSize; // Размер массива смещений
    size_t lineBorder; // Граница-отступ, до которой может доходить итератор в каждой строке
    if(k == 3) {
        adjSize = 8;
        adjacentPositions = new size_t[adjSize]{0, 1, 2, w, w+2, 2*w, 2*w+1, 2*w+2};
        lineBorder = w-2;
    }
    else if(k == 5) {
        adjSize = 24;
        adjacentPositions = new size_t[adjSize]{0, 1, 2, 3, 4, w, w+1, w+2, w+3, w+4, 2*w, 2*w+1, 2*w+3, 2*w+4, 3*w, 3*w+1, 3*w+2, 3*w+3, 3*w+4, 4*w, 4*w+1, 4*w+2, 4*w+3, 4*w+4};
        lineBorder = w-4;
    } else {
        return nullptr;
    }
    const size_t comparedPosition = adjacentPositions[adjSize - 1] / 2; // Смещение для проверяемого пикселя(центрального в квадрате k*k)
    uint32 sum; // Сумма значений пикселей для вычисления среднего
    QSet<size_t>* brokenPixels = new QSet<size_t>;

    // Итерация изображения по строкам до w-(k-1) и до h-(k-1) по столбцам
    for(size_t i = 0; i < npixels-adjacentPositions[adjSize - 1]; i++) {
        if(!(i%w < lineBorder)) // Если итератор вышел за границу строки - пропускаем(исключаем выход квадрата за пределы изображения)
            continue;
        sum = 0;
        for(uint8 pos = 0; pos < adjSize; pos++) { // Суммирования всех пикселей в квадрате k*k кроме цетрального
                sum += raster[i + adjacentPositions[pos]];
        }

        // Если отличие превышает заданный порог, то индекс пикселя помещается в коллекцию
        if(isExceedThreshold(sum/adjSize - raster[i + comparedPosition], threshold))
            *brokenPixels << i + comparedPosition;
    }
    delete[] adjacentPositions;
    return brokenPixels;
}

// Поиск медианы
uint16 median(uint16 f, uint16 s, uint16 t) {
    if(f < s){
        if(s < t) return s;
        if(f < t) return t;
        return f;
    }
    if(t < s) return s;
    if(f < t) return f;
    return t;
}

/*!
 * \brief Поиск битых пикселей посредством сравнения с медианой пикселей(реализовано для квадрата 3*3)
 * \param raster - массив пикселей
 * \param w - ширина изображения
 * \param npixels - количество пикселей
 * \param threshold - порог по которому будут отбираться искомые пиксели
 * \return Возвращается коллекция индексов пикселей отобранных алгоритмом.
 */
QSet<size_t>* medianBrokenPixelSearch(uint16* raster, uint32 w, size_t npixels, const uint16 threshold) {
    /* Смещения для пар диаметрально противоложных пикселей
     * число - индекс пары в массиве
     * p - центральный пиксель, является третим элементом вместе с парой при поиске медианы
     * 3 1 2
     * 0 p 0
     * 2 1 3
     */
    const size_t oppositePairsPoss[4][2] {{w, w+2}, {1, 2*w+1}, {0, 2*w+2}, {2*w, 2}};
    const size_t cPos = w+1; // Позиция проверяемого пикселя
    uint16 cPixel; // Значение проверяемого пикселя
    QSet<size_t>* brokenPixels = new QSet<size_t>;
    int32 delta; // Отличие вычисляемого и фактического значения пикселя
    // Итерация изображения по строкам до w-2 и до h-2 по столбцам
    for(size_t i = 0; i < npixels-w-w-2; i++) {
        if(!(i%w < w-2)) // Исключаем выход квадрата за пределы изображения
            continue;
        cPixel = raster[i + cPos];

        /* Поиск медианы пикселей происходит в порядке:
         * 1. Для каждой пары вместе с центральным пикселем
         * 2. Для медиан пар 0 1 и центрального пикселя и медиан пар 2 3 и центрального пикселя
         * 3. Для медиан из пункта 2 и центрального пикселя
         */
        delta = median(//пункт 3
            cPixel,
            median(//пунтк 2
                cPixel,
                median( cPixel, raster[i + oppositePairsPoss[0][0]], raster[i + oppositePairsPoss[0][1]]),//пункт 1
                median( cPixel, raster[i + oppositePairsPoss[1][0]], raster[i + oppositePairsPoss[1][1]]) //пункт 1
            ),//п.2
            median(//пункт 2
                cPixel,
                median( cPixel, raster[i + oppositePairsPoss[2][0]], raster[i + oppositePairsPoss[2][1]]),//пункт 1
                median( cPixel, raster[i + oppositePairsPoss[3][0]], raster[i + oppositePairsPoss[3][1]]) //пункт 1
            )//п.2
        );/*п.3*/

        // Если отличие превышает заданный порог, то индекс пикселя помещается в коллекцию
        if(isExceedThreshold(delta - raster[i + cPos], threshold))
            *brokenPixels << i + cPos;
    }
    return brokenPixels;
}

/*!
 * \brief Поиск битых пикселей методом иерархий(реализовано для квадрата 5*5)
 * \param raster - массив пикселей
 * \param w - ширина изображения
 * \param npixels - количество пикселей
 * \param threshold - порог по которому будут отбираться искомые пиксели
 * \return Возвращается коллекция индексов пикселей отобранных алгоритмом.
 */
QSet<size_t>* hierarchyBrokenPixelSearch(uint16* raster, uint32 w, size_t npixels, const uint16 threshold) {
    /* Алгоритм подбирает подходящий цвет основываясь на сумме весовых коэффициентах 3-х критериев
     *
     * Критерий 1
     * 1.Для каждого соседа вычисяется среднее значение его соседей без учета проверяемого
     * 2.Считается отличие среднего значения каждого соседа от максимального значения цвета и переводится в значение диапазона [0,1]
     * 3.Для каждого соседа вычисляется вес значений из п.2(значение делится на сумму всех значений)
     *
     * Критерий 2
     * 1.Для каждого соседа вычисляется количество пикселей такого же цвета без учета проверяемого
     * 2.Для каждого соседа вычисляется вес значений и п.1(значение делится на сумму всех значений)
     *
     * Критерий 3
     * 1.Для каждой пары противоположных соседей считается разница значений цветов
     * 2.Считается отличие разницы значений каждой пары от максимального значения цвета и переводится в значение диапазона [0,1]
     * 3.Для каждого соседа вычисляется вес значений из п.2(значение делится на сумму всех значений)
     */

    /* Для оптимизации, алгоритм был преобразован следующим образом:
     * К1: Т.к. среднее значение окружающих пикселей может использоваться до 8 раз,
     * сначала считаются суммы окружающих пикселей для каждого пикселя изображения(исключая полосу в 1 пиксель по краям изображения).
     * Таким образом для вычисления ср. знач. достаточно вычесть значение исключаемого пикселя и поделить на 7.
     * Вес среднего значения отличия пикселя от максимального значения цвета(мзц) - это отношение разницы мзц и среднего значения пикселя к разнице мзц*7 и суммы средних значений всех соседей.
     * Умножение на 7 получается из суммы отличий всех пикселей(всего окружающих пикселей 8), кроме проверяемого пикселя.
     *
     * К2: Т.к. количество соседей такого же пикселя может использоваться до 8 раз,
     * сначала считается количество таких же пикселей среди соседей для каждого пикселя изображения(исключая полосу в 1 пиксель по краям изображения).
     * Вес количества похожих пикселей расчитывается как отношение этого значения для соседнего пикселя к сумме значений всех соседей.
     *
     * К3: Разница противоположных пикселей вычисляется 4 раза - для каждой пары.
     * Значение суммы увеличивается в 2 раза т.к. для корректной работы разница должна быть высчитана для всех 8 пикселей
     * Вес расчитывается как отношение разницы мзц и отличия пикселей в паре к сумме отличий.
     */
    QSet<size_t>* brokenPixels = new QSet<size_t>;
    /* Смещения индекса пикселя для выбора соседних пикселей
     * 0 - текущий пиксель (на индекс которого указывает переменная итератор)
     * p - пиксель со значением которого будет производится сравнение
     * 5 6 7
     * 3 p 4
     * 0 1 2
     */
    size_t adjacentPositions[8] = {0, 1, 2, w, w+2, 2*w, 2*w+1, 2*w+2};
    size_t comparedPixel = w+1;// Смещение для проверяемого пикселя(центрального в квадрате 3*3)
    uint32* sumsRaster = new uint32[npixels]; // Суммы соседних пикселей для каждого пикселя
    uint8* samePixels = new uint8[npixels]; // Количество соседних пикселей того же цвета для каждого пикселя

    // Предварительный этап
    // 1.Вычисляются суммы соседей и количество таких же пикселей для всего изображения, исключая полосы в 1 пиксель по краям изображения
    for(size_t i = 0; i < npixels - 2*w-2; i++) {
        if(!(i%w < w-2)) // Исключаем выход квадрата за пределы изображения
            continue;
        sumsRaster[i + comparedPixel] = 0;
        samePixels[i + comparedPixel] = 0;
        for(uint8 pos = 0; pos < 8; pos++) {
            //Вычисляем сумму всех соседей для текущего центрального пикселя квадрата
            sumsRaster[i + comparedPixel] += raster[i + adjacentPositions[pos]];
            //Считаем количество всех соседей имеющих такое же значение цвета для текущего центрального пикселя квадрата
            if(raster[i + comparedPixel] == raster[i + adjacentPositions[pos]])
                samePixels[i + comparedPixel]++;
        }
    }
    // 2.Задаются константы мзц для одного значения, 7 (при вычислении сыммых средних), 4 (при вычислении суммы разниц)
    const uint32 M = 0xffff, avgM = M*7, diffM = M*4;

    double avgNeighborPixel[8]; // Средние значения окружающих пикселей, исключая проверяемый, для 8 пикселей соседей
    double sumAvgNeighborPixels; // Сумма средних значений соседних пикселей
    size_t checkingPos, oppositeCheckingPos; // Позиция обрабатываемого соседнего пикселя; позиция потивоположного checkingPos пикселя относительно центрального
    double P[8]; // Веса пикселей (общий, по критерию 1)

    double samePixelsSum; // Сумма значений для соседних пикселей из массива samePixels
    double V[8]; // Веса пикселей (по критерию 2)

    uint16 diffOppositePixels[4]; // Разница противолежащих пикселей
    double sumDiffs; // Сумма разниц
    double W[8]; // Веса пикселей (по критерию 3)

    // Итерация изображения по строкам до w-2 и до h-2 по столбцам
    for(size_t i = 0; i < npixels - 2*w-2; i++){
        if(!(i%w < w-2)) // Если итератор вошёл в отступы строки - пропускаем(исключаем выход квадрата за пределы изображения)
            continue;
        for(uint8 a = 0; a < 8; a++){
            P[a] = 0;
            V[a] = 0;
            W[a] = 0;
        }

        // Для вычисления сумм отличий от мзц из суммы мзц будем вычитать значения для каждого соседа
        sumAvgNeighborPixels = avgM;
        samePixelsSum = 0;
        sumDiffs = diffM;
        // Составление массива значений и их сумм для каждого критерия
        for(uint8 dir = 0; dir < 8; dir++){
            // Смещение для текущего обрабатываемого соседа
            checkingPos = i + adjacentPositions[dir];
            // Вычисление среднего значения для соседей, исключая проверяемый пиксель
            avgNeighborPixel[dir] = (sumsRaster[checkingPos] - raster[i + comparedPixel]) / 7.0;
            sumAvgNeighborPixels -= avgNeighborPixel[dir];
            // Вычисляем количество похожих пикселей из окружения
            if(samePixels[i + adjacentPositions[dir]] != 0){
                // Если значение проверяемого пикселя учлось, то исключаем его из общего количества
                V[dir] = raster[i + comparedPixel] == raster[checkingPos] ? samePixels[i + adjacentPositions[dir]] - 1 : samePixels[i + adjacentPositions[dir]];
                samePixelsSum += V[dir];
            }
            // Вычисляем отличие для пар противоположных пикселей
            if(dir < 4) {
                // Смещение для противоположного соседа
                oppositeCheckingPos = i + adjacentPositions[7 - dir];
                // Вычисление модуля разницы пикселей в паре
                if(raster[checkingPos] > raster[oppositeCheckingPos]) {
                    diffOppositePixels[dir] = raster[checkingPos] - raster[oppositeCheckingPos];
                    sumDiffs -= diffOppositePixels[dir];
                }
                else {
                    diffOppositePixels[dir] = raster[oppositeCheckingPos] - raster[checkingPos];
                    sumDiffs -= diffOppositePixels[dir];
                }
            }
        }
        // Получаем сумму для 8 соседей
        sumDiffs *= 2;
        // Расчитываем веса
        for(uint8 dir = 0; dir < 8; dir++) {
            // Вес по критерию 1
            P[dir] += (M - avgNeighborPixel[dir]) / sumAvgNeighborPixels;
            // Вес по критерию 2
            if(V[dir] != 0){
                P[dir] += V[dir] / samePixelsSum;
            }
            // Вес по критерию 3
            if(dir < 4) {
                W[dir] = (M - diffOppositePixels[dir]) / sumDiffs;
                P[dir] += W[dir];
                // Т.к значения весов одинаковы для пикселей в паре, добавляем такой же вес противоположному пикселю
                P[7 - dir] += W[dir];
            }
        }

        // Если отличие превышает заданный порог, то индекс пикселя помещается в коллекцию
        if(isExceedThreshold(int32(raster[i + comparedPixel]) - int32(raster[i + adjacentPositions[max_element(P, P+8) - P]]), threshold)) {
            *brokenPixels << i + comparedPixel;
        }
    }

    // delete
    delete[] sumsRaster;
    delete[] samePixels;
    return brokenPixels;
}


int main(int argc, char* argv[])
{
    char* path;
    uint16 threshold;
    if(argc != 3) {
        cout << "Enter path to img and threshold as a percentage\nExample: \"img.tif\" 25" << endl;
        return 0;
    }
    else {
        path = argv[1];
        string str = argv[2];
        for(auto i : str) {
            if(!isdigit(i)) {
                cout << "Error: threshold is number" << endl;
                return 0;
            }
        }
        if(atof(argv[2]) > 0 && atof(argv[2]) < 100)
            threshold = 0xffff * (atoi(argv[2]) / 100.0);
        else {
            cout << "Error: threshold shoud be between 0 and 100" << endl;
            return 0;
        }
    }
    time_t start, end;

    uint16* raster = nullptr; uint32 w = 0, h = 0; size_t npixels = 0;
    uint8 numberOfMethods = 4;
    QSet<size_t>** brokenPixels = new QSet<size_t>*[numberOfMethods]{nullptr};
    QSet<size_t> resultBrokenPixelsSet;

    uint8 errCode = getImage(path, raster, w, h, npixels);
    if(errCode == 0) {
        start = clock();
        brokenPixels[0] = avgBrokenPixelSearch(raster, w, npixels, threshold, 3);
        end = clock();
        cout << "avg3 milliseconds: " << end - start << endl;

        start = clock();
        brokenPixels[1] = avgBrokenPixelSearch(raster, w, npixels, threshold, 5);
        end = clock();
        cout << "avg5 milliseconds: " << end - start << endl;

        start = clock();
        brokenPixels[2] = medianBrokenPixelSearch(raster, w, npixels, threshold);
        end = clock();
        cout << "median3 milliseconds: " << end - start << endl;

        start = clock();
        brokenPixels[3] = hierarchyBrokenPixelSearch(raster, w, npixels, threshold);
        end = clock();
        cout << "hierarchy3 milliseconds: " << end - start << endl;

        for(uint8 method = 0; method < numberOfMethods; method++) {
            resultBrokenPixelsSet.unite(*(brokenPixels[method]));
        }
        QList<size_t> outputList(resultBrokenPixelsSet.begin(), resultBrokenPixelsSet.end());
        sort(outputList.begin(), outputList.end());

        cout << "Pixels total: " << outputList.count() << endl;
        cout << setw(11) << setfill(' ') << "(w;h)";
        for(uint8 method = 0; method < numberOfMethods; method++) {
            cout << setw(9) << setfill(' ') << "Method " + to_string(method);
        }
        cout << endl;
        double counter;
        for(auto el: outputList){
            cout << setw(11) << setfill(' ') << "(" + to_string(el%w) + ";" + to_string(el/w) + ")";
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
    }
    else {
        switch (errCode) {
        case 1:
            cout << "Error: couldn't open the file" << endl;
            break;
        case 2:
            cout << "Error: image configuration is not supported" << endl;
            break;
        case 3:
            cout << "Error: image is too small" << endl;
            break;
        case 4:
            cout << "Error: failed to allocate memory" << endl;
            break;
        case 5:
            cout << "Error: couldn't read the image" << endl;
            break;
        }
    }

    delete path;
    delete[] raster;
    for(uint8 i = 0; i < numberOfMethods; i++)
        delete brokenPixels[i];
    delete[] brokenPixels;
}
