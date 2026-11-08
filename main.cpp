#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <thread>
#include <latch>
#include <mutex>
#include <chrono>
#include <format>

using namespace std;

// Варіант 8: m = 3
//            поле 0        поле 1        поле 2
//          read  write   read  write   read  write   string
//          20%   10%     25%   10%     20%   10%     5%
//
// Схема захисту: кожне поле має власний mutex (3 мʼютекси).
//   read  <i>      - блокується лише мʼютекс поля i
//   write <i> <v>  - блокується лише мʼютекс поля i
//   string         - блокуються всі 3 мʼютекси одночасно через scoped_lock (без взаємоблокувань)

class Data
{
public:
    static const int m = 3;

    int get(int i) const
    {
        lock_guard lock(fields[i].mtx);
        return fields[i].value;
    }

    void set(int i, int value)
    {
        lock_guard lock(fields[i].mtx);
        fields[i].value = value;
    }

    operator string() const
    {
        int v0, v1, v2;
        {
            scoped_lock lock(fields[0].mtx, fields[1].mtx, fields[2].mtx);
            v0 = fields[0].value;
            v1 = fields[1].value;
            v2 = fields[2].value;
        }
        return format("{} {} {}", v0, v1, v2);
    }

private:
    // кожне поле разом зі своїм мʼютексом в окремій кеш-лінії
    struct alignas(64) Field
    {
        int value = 0;
        mutable mutex mtx;
    };

    Field fields[m];
};

// ---------------- демо-код ----------------

struct Op
{
    char type;  // 'r' - read, 'w' - write, 's' - string
    int field;
    int value;
};

vector<Op> load(const string& path)
{
    vector<Op> ops;
    ifstream in(path);
    string cmd;
    while (in >> cmd)
    {
        Op op{};
        if (cmd == "write")
        {
            op.type = 'w';
            in >> op.field >> op.value;
        }
        else if (cmd == "read")
        {
            op.type = 'r';
            in >> op.field;
        }
        else
        {
            op.type = 's';
        }
        ops.push_back(op);
    }
    return ops;
}

long long execute(Data& data, const vector<Op>& ops)
{
    long long check = 0;  // щоб результати операцій не були викинуті оптимізатором
    for (const Op& op : ops)
    {
        switch (op.type)
        {
            case 'r': check += data.get(op.field); break;
            case 'w': data.set(op.field, op.value); break;
            case 's': check += string(data).size(); break;
        }
    }
    return check;
}

// час виконання (мс) файлів tests/<name>_<nthreads>_<t>.txt, кожен у своєму потоці
double measure(const string& name, int nthreads)
{
    vector<vector<Op>> ops;
    for (int t = 1; t <= nthreads; t++)
    {
        ops.push_back(load(format("tests/{}_{}_{}.txt", name, nthreads, t)));
    }

    Data data;
    latch start(1);
    vector<long long> checks(nthreads);
    chrono::steady_clock::time_point t0;
    {
        vector<jthread> vt;
        for (int t = 0; t < nthreads; t++)
        {
            vt.emplace_back([&, t]() {
                start.wait();
                checks[t] = execute(data, ops[t]);
            });
        }
        t0 = chrono::steady_clock::now();
        start.count_down();
    }
    chrono::duration<double, milli> d = chrono::steady_clock::now() - t0;
    return d.count();
}

int main()
{
    const int repeats = 10;
    const string names[] = {"a", "b", "c"};
    const string titles[] = {
        "a) частоти за варіантом",
        "b) рівні частоти",
        "c) частоти, що сильно відрізняються",
    };

    cout << format("Середній час виконання, мс (усереднено по {} запусках)\n\n", repeats);
    cout << format("{:<40}{:>12}{:>12}{:>12}\n", "", "1 потік", "2 потоки", "3 потоки");
    for (int k = 0; k < 3; k++)
    {
        cout << format("{:<40}", titles[k]);
        for (int nthreads = 1; nthreads <= 3; nthreads++)
        {
            double sum = 0;
            for (int r = 0; r < repeats; r++)
            {
                sum += measure(names[k], nthreads);
            }
            cout << format("{:>12.2f}", sum / repeats);
        }
        cout << "\n";
    }
}
