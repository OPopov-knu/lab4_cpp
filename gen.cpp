#include <fstream>
#include <random>
#include <format>
#include <filesystem>

using namespace std;

// Генерує тестові файли tests/<name>_<nthreads>_<t>.txt
// Загальна кількість операцій N однакова для 1, 2 і 3 потоків (кожен потік отримує N / nthreads).

const int N = 300000;

// ваги: read 0, write 0, read 1, write 1, read 2, write 2, string
void generate(const string& name, initializer_list<double> weights, mt19937& gen)
{
    discrete_distribution<int> kind(weights);
    uniform_int_distribution<int> value(0, 99);

    for (int nthreads = 1; nthreads <= 3; nthreads++)
    {
        for (int t = 1; t <= nthreads; t++)
        {
            ofstream out(format("tests/{}_{}_{}.txt", name, nthreads, t));
            for (int i = 0; i < N / nthreads; i++)
            {
                int k = kind(gen);
                if (k == 6)
                    out << "string\n";
                else if (k % 2 == 0)
                    out << "read " << k / 2 << "\n";
                else
                    out << "write " << k / 2 << " " << value(gen) << "\n";
            }
        }
    }
}

int main()
{
    mt19937 gen{random_device()()};
    filesystem::create_directory("tests");

    // a) частоти відповідають варіанту 8
    generate("a", {20, 10, 25, 10, 20, 10, 5}, gen);
    // b) усі частоти рівні
    generate("b", {1, 1, 1, 1, 1, 1, 1}, gen);
    // c) частоти дуже сильно відрізняються від варіанту: майже лише запис і string
    generate("c", {1, 25, 1, 25, 1, 25, 22}, gen);
}
