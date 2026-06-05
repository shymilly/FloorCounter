#include <iostream>
#include <chrono>
#include <numeric>
#include <filesystem>
#include <map>
#include <opencv2/opencv.hpp>

using namespace std;
using namespace cv;

// ================================
// ВЕРТИКАЛЬНЫЙ ПРОФИЛЬ
// ================================
vector<float> computeVerticalProfile(const Mat& gray, int stripWidthPercent = 20) {
    int height = gray.rows;
    int width = gray.cols;
    int stripWidth = max(1, width * stripWidthPercent / 100);
    int startX = (width - stripWidth) / 2;
    Rect roi(startX, 0, stripWidth, height);
    Mat strip = gray(roi);
    vector<float> profile(height);
    for (int y = 0; y < height; ++y) {
        profile[y] = static_cast<float>(mean(strip.row(y))[0]);
    }
    return profile;
}

// ================================
// СГЛАЖИВАНИЕ ПРОФИЛЯ (медианный фильтр)
// ================================
vector<float> smoothProfile(const vector<float>& signal, int windowSize = 5) {
    vector<float> smoothed(signal.size());
    int half = windowSize / 2;
    for (size_t i = 0; i < signal.size(); ++i) {
        float sum = 0;
        int count = 0;
        for (int j = -half; j <= half; ++j) {
            int idx = i + j;
            if (idx >= 0 && idx < (int)signal.size()) {
                sum += signal[idx];
                count++;
            }
        }
        smoothed[i] = sum / count;
    }
    return smoothed;
}

// ================================
// НОРМАЛИЗАЦИЯ СИГНАЛА
// ================================
vector<float> normalizeSignal(const vector<float>& signal) {
    float mean = std::accumulate(signal.begin(), signal.end(), 0.0f) / signal.size();
    vector<float> normalizedSignal(signal.size());
    for (size_t i = 0; i < signal.size(); ++i) {
        normalizedSignal[i] = signal[i] - mean;
    }
    return normalizedSignal;
}

// ================================
// АВТОКОРРЕЛЯЦИЯ
// ================================
vector<float> computeAutocorrelation(const vector<float>& signal, int maxLag) {
    int n = signal.size();
    vector<float> normSignal = normalizeSignal(signal);
    vector<float> corr(maxLag + 1, 0.0f);
    
    for (int lag = 1; lag <= maxLag && lag < n; ++lag) {
        float sum = 0.0f;
        int count = 0;
        for (int i = 0; i < n - lag; ++i) {
            sum += normSignal[i] * normSignal[i + lag];
            count++;
        }
        if (count > 0) {
            corr[lag] = sum / count;
        }
    }
    
    float maxVal = 0;
    for (int lag = 1; lag <= maxLag; ++lag) {
        if (corr[lag] > maxVal) maxVal = corr[lag];
    }
    
    if (maxVal > 1e-6f) {
        for (int lag = 1; lag <= maxLag; ++lag) {
            corr[lag] /= maxVal;
        }
    }
    
    return corr;
}

// ================================
// ПОИСК НЕСКОЛЬКИХ ПИКОВ
// ================================
vector<int> findPeaks(const vector<float>& corr, int minPeriod, int maxPeriod, float threshold = 0.5f) {
    vector<int> peaks;
    for (int period = minPeriod; period <= maxPeriod && period < (int)corr.size(); ++period) {
        if (corr[period] > threshold) {
            bool isPeak = true;
            int checkRange = 2;
            for (int d = -checkRange; d <= checkRange; ++d) {
                if (d != 0 && period + d >= minPeriod && period + d <= maxPeriod) {
                    if (corr[period + d] >= corr[period]) {
                        isPeak = false;
                        break;
                    }
                }
            }
            if (isPeak) {
                peaks.push_back(period);
            }
        }
    }
    return peaks;
}

// ================================
// ОПРЕДЕЛЕНИЕ ГРАНИЦ ЗДАНИЯ
// ================================
Rect detectBuildingBounds(const Mat& gray) {
    int height = gray.rows;
    int width = gray.cols;
    
    // Вертикальный профиль для поиска границ
    vector<float> profile = computeVerticalProfile(gray, 30);
    
    // Ищем верхнюю границу (небо -> здание)
    int top = height / 8;
    for (int y = height / 10; y < height / 2; ++y) {
        if (y > 5) {
            float diff = abs(profile[y] - profile[y-5]);
            if (diff > 40) {  // резкий перепад
                top = max(0, y - 15);
                break;
            }
        }
    }
    
    // Ищем нижнюю границу (здание -> земля)
    int bottom = height - height / 8;
    for (int y = height - height / 10; y > height / 2; --y) {
        if (y < height - 5) {
            float diff = abs(profile[y] - profile[y+5]);
            if (diff > 40) {  // резкий перепад
                bottom = min(height - 1, y + 15);
                break;
            }
        }
    }
    
    return Rect(0, top, width, bottom - top);
}

// ================================
// ОСНОВНАЯ ФУНКЦИЯ ПОДСЧЁТА
// ================================
int countFloors(const string& imagePath, int stripPercent = 20, 
                int minPeriod = 15, int maxPeriod = 50, 
                float threshold = 0.4f, bool useSmoothing = true) {
    Mat img = imread(imagePath);
    if (img.empty()) {
        return -1;
    }
    
    Mat gray;
    if (img.channels() == 3) {
        cvtColor(img, gray, COLOR_BGR2GRAY);
    } else {
        gray = img.clone();
    }
    
    // Обнаруживаем границы здания
    Rect building = detectBuildingBounds(gray);
    gray = gray(building);
    
    int height = gray.rows;
    if (height < 50) return -1;
    
    // Вертикальный профиль
    vector<float> profile = computeVerticalProfile(gray, stripPercent);
    
    // Сглаживание
    if (useSmoothing) {
        profile = smoothProfile(profile, 5);
    }
    
    // Автокорреляция
    vector<float> corr = computeAutocorrelation(profile, maxPeriod);
    
    // Поиск пиков
    vector<int> peaks = findPeaks(corr, minPeriod, maxPeriod, threshold);
    
    if (peaks.empty()) {
        return -1;
    }
    
    // Выбираем наиболее вероятный период
    int bestPeriod = peaks[0];
    float bestCorr = corr[bestPeriod];
    for (int p : peaks) {
        if (corr[p] > bestCorr) {
            bestCorr = corr[p];
            bestPeriod = p;
        }
    }
    
    // Расчёт этажей с ограничениями
    int rawFloors = height / bestPeriod;
    
    // Ограничиваем разумными значениями
    if (rawFloors > 20) rawFloors = 20;
    if (rawFloors < 1) rawFloors = 1;
    
    return rawFloors;
}

// ================================
// РАСШИРЕННАЯ ФУНКЦИЯ С ДИАГНОСТИКОЙ
// ================================
int countFloorsVerbose(const string& imagePath, int stripPercent = 20, 
                       int minPeriod = 15, int maxPeriod = 50, 
                       float threshold = 0.4f) {
    Mat img = imread(imagePath);
    if (img.empty()) {
        cout << "  Ошибка загрузки" << endl;
        return -1;
    }
    
    Mat gray;
    cvtColor(img, gray, COLOR_BGR2GRAY);
    
    Rect building = detectBuildingBounds(gray);
    cout << "  Границы здания: Y=" << building.y << " - " << (building.y + building.height) << endl;
    
    gray = gray(building);
    int height = gray.rows;
    cout << "  Высота здания: " << height << " px" << endl;
    
    vector<float> profile = computeVerticalProfile(gray, stripPercent);
    vector<float> smoothed = smoothProfile(profile, 5);
    vector<float> corr = computeAutocorrelation(smoothed, maxPeriod);
    
    vector<int> peaks = findPeaks(corr, minPeriod, maxPeriod, threshold);
    cout << "  Найдено пиков: " << peaks.size() << endl;
    
    if (peaks.empty()) return -1;
    
    cout << "  Периоды пиков: ";
    for (int p : peaks) cout << p << " ";
    cout << endl;
    
    int bestPeriod = peaks[0];
    float bestCorr = corr[bestPeriod];
    for (int p : peaks) {
        if (corr[p] > bestCorr) {
            bestCorr = corr[p];
            bestPeriod = p;
        }
    }
    
    cout << "  Лучший период: " << bestPeriod << " px (корр=" << bestCorr << ")" << endl;
    
    int rawFloors = height / bestPeriod;
    if (rawFloors > 20) rawFloors = 20;
    
    return rawFloors;
}

// ================================
// ГЛАВНАЯ ФУНКЦИЯ
// ================================
int main() {
    cout << "\n==================================================" << endl;
    cout << "   ПОДСЧЁТ ЭТАЖЕЙ - АВТОКОРРЕЛЯЦИЯ" << endl;
    cout << "   (адаптировано для маленьких зданий)" << endl;
    cout << "==================================================\n" << endl;
    
    string imagePath = "/home/shymilly/FloorCounter/my_building.jpg";
    
    if (!std::filesystem::exists(imagePath)) {
        cout << "ОШИБКА: Файл не найден: " << imagePath << endl;
        cout << "Укажите правильный путь в коде или создайте файл." << endl;
        return -1;
    }
    
    cout << "Изображение: " << imagePath << endl;
    
    Mat originalImg = imread(imagePath);
    if (originalImg.empty()) {
        cout << "ОШИБКА: Не удалось загрузить изображение" << endl;
        return -1;
    }
    
    cout << "Размер: " << originalImg.cols << "x" << originalImg.rows << endl;
    
    // НАБОР ПАРАМЕТРОВ ДЛЯ МАЛЕНЬКИХ ЗДАНИЙ
    vector<tuple<int, int, int, float, bool>> paramSets = {
        {20, 12, 35, 0.4f, true},   // 1 - маленький период
        {20, 10, 30, 0.45f, true},  // 2 - ещё меньше
        {20, 15, 40, 0.4f, true},   // 3 - средний
        {15, 12, 35, 0.4f, true},   // 4 - узкая полоса
        {30, 12, 35, 0.4f, true},   // 5 - широкая полоса
        {20, 12, 35, 0.35f, true},  // 6 - низкий порог
        {20, 12, 35, 0.5f, true},   // 7 - высокий порог
        {20, 12, 35, 0.4f, false},  // 8 - без сглаживания
        {25, 12, 40, 0.4f, true},   // 9 - адаптивный
        {20, 8, 25, 0.4f, true},    // 10 - очень маленький период
        {20, 12, 35, 0.4f, true},   // 11 - повтор
        {15, 10, 30, 0.45f, true}   // 12 - комбинированный
    };
    
    cout << "\n==================================================" << endl;
    cout << "   ТЕСТИРОВАНИЕ (" << paramSets.size() << " вариантов)" << endl;
    cout << "==================================================\n" << endl;
    
    map<int, int> freq;
    vector<double> times;
    
    for (size_t i = 0; i < paramSets.size(); ++i) {
        int strip = get<0>(paramSets[i]);
        int minP = get<1>(paramSets[i]);
        int maxP = get<2>(paramSets[i]);
        float thresh = get<3>(paramSets[i]);
        bool smooth = get<4>(paramSets[i]);
        
        auto start = chrono::high_resolution_clock::now();
        int floors = countFloors(imagePath, strip, minP, maxP, thresh, smooth);
        auto end = chrono::high_resolution_clock::now();
        double timeMs = chrono::duration<double, milli>(end - start).count();
        
        times.push_back(timeMs);
        if (floors > 0) {
            freq[floors]++;
        }
        
        cout << "[" << (i+1) << "] полоса=" << strip << "%, период=[" << minP << "," << maxP 
             << "], порог=" << thresh << ", сглаживание=" << (smooth ? "да" : "нет")
             << " → " << floors << " эт (" << timeMs << " ms)" << endl;
    }
    
    // Подробная диагностика для лучшего варианта
    cout << "\n==================================================" << endl;
    cout << "   ДИАГНОСТИКА (лучшие параметры)" << endl;
    cout << "==================================================" << endl;
    
    int bestFloors = countFloorsVerbose(imagePath, 20, 12, 35, 0.4f);
    
    // Статистика
    cout << "\n==================================================" << endl;
    cout << "   СТАТИСТИКА" << endl;
    cout << "==================================================" << endl;
    
    int mostLikely = -1;
    int maxCount = 0;
    for (const auto& [floors, count] : freq) {
        if (count > maxCount) {
            maxCount = count;
            mostLikely = floors;
        }
    }
    
    double avgTime = 0;
    for (double t : times) avgTime += t;
    avgTime /= times.size();
    
    cout << "Наиболее вероятное количество этажей: " << mostLikely << endl;
    cout << "Среднее время обработки: " << avgTime << " ms" << endl;
    cout << "Всего тестов: " << paramSets.size() << endl;
    
    // Визуализация
    cout << "\n==================================================" << endl;
    cout << "   ВИЗУАЛИЗАЦИЯ" << endl;
    cout << "==================================================" << endl;
    
    Mat displayImg = originalImg.clone();
    string text = "Floors: " + to_string(mostLikely);
    putText(displayImg, text, Point(10, 50), FONT_HERSHEY_SIMPLEX, 1.2, Scalar(0, 255, 0), 2);
    
    if (displayImg.cols > 800) {
        Mat resized;
        resize(displayImg, resized, Size(800, 800 * displayImg.rows / displayImg.cols));
        displayImg = resized;
    }
    
    imshow("Building Floors", displayImg);
    cout << "\nНажмите любую клавишу для выхода..." << endl;
    waitKey(0);
    destroyAllWindows();
    
    return 0;
}
