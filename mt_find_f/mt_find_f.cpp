#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <thread>
#include <mutex>
#include <regex>
#include <atomic>
#include <algorithm> // Для сортировки результатов
/*
 Написать программу mtfind, производящую поиск подстроки в текстовом файле по маске с использованием многопоточности.
 Маска - это строка, где "?" обозначает любой символ.
 Программа принимает в качестве параметров командной строки:
 1. Имя текстового файла, в котором должен идти поиск (размер файла - до 1Гб).
 2. Маску для поиска, в кавычках. Максимальная длина маски 1000 символов.
 Вывод программы должен быть в следующем формате:
 На первой строке - количество найденных вхождений.
 Далее информация о каждом вхождении, каждое на отдельной строке, через пробел: номер строки, позиция в строке, само
 найденное вхождение.
 Порядок вывода найденных вхождений должен совпадать с их порядком в файле
 Вся нумерация ведется начиная с 1 (делаем программу для обычных людей)
 Дополнения:
 В текстовом файле кодировка только 7-bit ASCII
 Поиск с учетом регистра
 Каждое вхождение может быть только на одной строке. Маска не может содержать символа перевода строки
 Найденные вхождения не должны пересекаться. Если в файле есть пересекающиеся вхождения то нужно вывести одно из них
 (любое).
 Пробелы и разделители участвуют в поиске наравне с другими символами.
 Можно использовать STL, Boost, возможности С++1x, C++2x.
 Многопоточность нужно использовать обязательно. Однопоточные решения засчитываться не будут.
 Серьезным плюсом будет разделение работы между потоками равномерно вне зависимости от количества строк во входном
 файле
*/
// Структура для хранения результата
struct MatchResult {
    size_t line_number = 0;
    size_t position = 0;
    std::string match = "";
};

// Мьютекс для синхронизации добавления результатов
std::mutex result_mutex;
std::vector<MatchResult> results;

// Функция для поиска подстроки по маске в заданных строках
void search_in_lines
(const std::vector<std::string>& lines,
    size_t start_line,
    size_t end_line,
    const std::regex& pattern,
    std::atomic<size_t>& match_count)
{
    for (size_t i = start_line; i < end_line; ++i) {
        const auto& line = lines[i];
        auto it = std::sregex_iterator(line.begin(), line.end(), pattern);
        auto end = std::sregex_iterator();

        for (; it != end; ++it) {
            std::lock_guard<std::mutex> lock(result_mutex);
            results.push_back({ i + 1, static_cast<size_t>(it->position()) + 1, it->str() });
            ++match_count;
        }
    }
}

// Функция для подготовки шаблона поиска
std::regex prepare_pattern(const std::string& mask) {
    std::string regex_pattern;
    for (char ch : mask) {
        if (ch == '?') {
            regex_pattern += '.'; // Любой символ
        }
        else {
            regex_pattern += std::regex_replace(std::string(1, ch), std::regex(R"([\\^$.|?*+()

\[\]

{}])"), R"(\\$&)");
        }
    }
    return std::regex(regex_pattern);
}

int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::cerr << "Usage: mtfind <filename> <mask>\n";
        return 1;
    }

    std::string filename = argv[1];
    std::string mask = argv[2];

    // Открываем файл
    std::ifstream file(filename);
    if (!file) { // проверка на открытие
        std::cerr << "Error: Cannot open file " << filename << "\n";
        return 1;
    }

    // Читаем файл построчно
    std::vector<std::string> lines;
    std::string line;
    while (std::getline(file, line)) {
        lines.push_back(line);
    }

    // Подготовка шаблона для поиска
    std::regex pattern = prepare_pattern(mask);

    // Определяем количество потоков
    const size_t num_threads = std::thread::hardware_concurrency();
    const size_t lines_per_thread = (lines.size() + num_threads - 1) / num_threads;

    // Создаем потоки для поиска
    std::vector<std::thread> threads;
    std::atomic<size_t> match_count(0);

    for (size_t i = 0; i < num_threads; ++i) {
        size_t start_line = i * lines_per_thread;
        size_t end_line = std::min(start_line + lines_per_thread, lines.size());
        if (start_line < lines.size()) {
            threads.emplace_back(search_in_lines, std::cref(lines), start_line, end_line, std::cref(pattern), std::ref(match_count));
        }
    }

    // Ждем завершения потоков
    for (auto& t : threads) {
        if (t.joinable()) {
            t.join();
        }
    }

    // Сортируем результаты по номеру строки
    std::sort(results.begin(), results.end(), [](const MatchResult& a, const MatchResult& b) {
        return a.line_number < b.line_number;
        });

    // Выводим результаты
    std::cout << match_count << "\n";
    for (const auto& result : results) {
        std::cout << result.line_number << " " << result.position << " " << result.match << "\n";
    }

    return 0;
}
