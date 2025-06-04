// 35. Дана прямоугольная целочисленная матрица. Распараллеливание по строкам.
// Найти количество строк, состоящих только из четных элементов.
// 4 задача: атомарные типы
// 5 задача: Queue, Mutex C++

#include <iostream>
#include <fstream>
#include <process.h>
#include <Windows.h>
#include <thread>
#include <future>
#include <atomic>
#include "ThreadsafeQueue.h"
#include <random>
#include <omp.h>

constexpr const size_t COLS = 5; // Количество столбцов (размер блока)
constexpr const size_t ROWS = 15;  // Количество строк
constexpr const size_t MATR_SIZE = COLS * ROWS; // Размер матрицы
constexpr const size_t NTHREADS = 4; //Кол-во потоков (распараллеливание по строкам)
constexpr const size_t BLOCK = ROWS / NTHREADS;

struct INFORM
{
	int** matrix;
	size_t left, right;
	int cnt;
};


// matrix interface
int** memory_allocation()
{
	int** matrix = new int* [ROWS];
	for (int i{}; i < ROWS; ++i)
		matrix[i] = new int[COLS];
	return matrix;
}

void free_memory(int**& matrix)
{
	for (int i{}; i < ROWS; ++i)
		delete[]matrix[i];
	delete[]matrix;
}

void init_matrix(int** matrix, int left, int right)
{
	for (size_t i{}; i < ROWS; ++i)
		for (size_t j{}; j < COLS; ++j)
			if (i % 2 == 0)
				matrix[i][j] = (left + rand() % (right - left)) * 2;
			else
				matrix[i][j] = left + rand() % (right - left);
}

void init_matrix(int** matrix, std::ifstream& file)
{
	for (size_t i{}; i < ROWS; ++i)
		for (size_t j{}; j < COLS; ++j)
			file >> matrix[i][j];
}

void print_matrix(int** matrix)
{
	for (size_t i{}; i < ROWS; ++i)
	{
		for (size_t j{}; j < COLS; ++j)
			std::cout << matrix[i][j] << ' ';
		std::cout << '\n';
	}
}


// common task
int all_even_rows_count(int** matrix, size_t left, size_t right)
{
	int count{};
	for (size_t i = left; i < right; ++i)
	{
		bool result = true;
		int j{};
		while (j < COLS && result)
		{
			if (matrix[i][j] % 2 != 0)
				result = false;
			j++;
		}
		if (result)
			count++;
	}
	return count;
}

bool is_all_even(int* arr)
{
	for (size_t i{}; i < COLS; ++i)
		if (arr[i] % 2 != 0)
			return false;
	return true;
}



 // task 1
unsigned __stdcall is_all_even_parallel(void* arg) // one thread
{
	INFORM* inform = (INFORM*)arg;
	int count{};
	for (size_t i = inform->left; i < inform->right; ++i)
	{
		if (is_all_even(inform->matrix[i]))
			count++;
	}
	inform->cnt = count;
	return 0;
}

int all_even_rows_count_parallel(int** matrix)
{
	HANDLE t[NTHREADS - 1];
	INFORM inform[NTHREADS];
	for (size_t i{}; i < NTHREADS; ++i)
	{
		inform[i].matrix = matrix;
		inform[i].left = i * BLOCK;
		if (i == NTHREADS - 1)
			inform[i].right = ROWS;
		else
		{
			inform[i].right = (i + 1) * BLOCK;
			t[i] = (HANDLE)_beginthreadex(nullptr, 0, &is_all_even_parallel, inform + i, 0, nullptr);
		}
	}
	is_all_even_parallel(&inform[NTHREADS - 1]); // Последний поток обрабатывает оставшиеся строки
	
	WaitForMultipleObjects(NTHREADS - 1, t, true, INFINITE);

	int count{};
	for (size_t i{}; i < NTHREADS; ++i)
	{
		count += inform[i].cnt;
		if (i < NTHREADS - 1)
			CloseHandle(t[i]);
	}
	return count;
}

// task 2
int all_even_rows_count_ref(int** matrix, size_t left, size_t right, int& count)
{
	count = 0;
	for (size_t i = left; i < right; ++i)
	{
		bool result = true;
		int j{};
		while (j < COLS && result)
		{
			if (matrix[i][j] % 2 != 0)
				result = false;
			j++;
		}
		if (result)
			count++;
	}
	return count;
}

int all_even_rows_count_parallel2(int** matrix)
{
	std::thread t[NTHREADS - 1];
	int results[NTHREADS - 1];
	
	for (size_t i{}; i < NTHREADS - 1; ++i)
	{
		t[i] = std::thread(all_even_rows_count_ref, matrix, i * BLOCK, (i + 1) * BLOCK, std::ref(results[i]));
	}

	int count{};
	all_even_rows_count_ref(matrix, (NTHREADS - 1) * BLOCK, ROWS, count); // Главный поток обрабатывает оставшиеся строки

	for (size_t i = 0; i < NTHREADS - 1; ++i) // sync
		t[i].join(); 

	for (size_t i = 0; i < NTHREADS - 1; ++i)
		count += results[i];

	return count;
}

int all_even_rows_count_parallel3(int** matrix)  
{  
    std::future<int> t[NTHREADS - 1];  

    for (size_t i{}; i < NTHREADS - 1; ++i)  
    {  
        t[i] = std::async(std::launch::async, all_even_rows_count, matrix, i * BLOCK, (i + 1) * BLOCK);  
    }  

    int count{};  
    count += all_even_rows_count(matrix, (NTHREADS - 1) * BLOCK, ROWS); // Главный поток обрабатывает оставшиеся строки  

    for (size_t i{}; i < NTHREADS - 1; ++i)  
    {  
        count += t[i].get();  
    }  

    return count;  
}

// task 4 (atomic)
int all_even_rows_count_atomic(int** matrix, size_t left, size_t right, std::atomic_int& count)
{
	count = 0;
	for (size_t i = left; i < right; ++i)
	{
		bool result = true;
		int j{};
		while (j < COLS && result)
		{
			if (matrix[i][j] % 2 != 0)
				result = false;
			j++;
		}
		if (result)
			count++;
	}
	return count;
}

int all_even_rows_count_parallel4(int** matrix)
{
	std::thread t[NTHREADS - 1];
	std::atomic_int results[NTHREADS - 1];

	for (size_t i{}; i < NTHREADS - 1; ++i)
	{
		t[i] = std::thread(all_even_rows_count_atomic, matrix, i * BLOCK, (i + 1) * BLOCK, std::ref(results[i]));
	}

	for (size_t i = 0; i < NTHREADS - 1; ++i)
		t[i].join();

	int count{};
	count += all_even_rows_count(matrix, (NTHREADS - 1) * BLOCK, ROWS); // Главный поток обрабатывает оставшиеся строки

	for (size_t i = 0; i < NTHREADS - 1; ++i)
		count += results[i];

	return count;
}

// task 5 (Queue, Mutex C++)
ThreadsafeQueue<std::pair<size_t, size_t>> que_int;

void all_even_rows_count_queue(int** matrix, std::atomic_int& result)
{
	std::pair<size_t, size_t> pos{};
	if (que_int.try_pop(pos)) // извлекаем индекс строки из очереди
	{
		result += all_even_rows_count(matrix, pos.first, pos.second);
	}
}

int all_even_rows_count_parallel5(int** matrix)
{
	std::thread t[NTHREADS - 1];
	std::atomic_int results[NTHREADS - 1];

	// Заполняем очередь
	for (size_t i{}; i < NTHREADS - 1; ++i)
		que_int.push(std::pair<size_t, size_t>(i * BLOCK, (i + 1) * BLOCK));

	for (size_t i{}; i < NTHREADS - 1; ++i)
	{
		t[i] = std::thread(all_even_rows_count_queue, matrix, std::ref(results[i]));
	}

	for (size_t i = 0; i < NTHREADS - 1; ++i)
		t[i].join();

	int count{};
	count += all_even_rows_count(matrix, (NTHREADS - 1) * BLOCK, ROWS); // Главный поток обрабатывает оставшиеся строки

	for (size_t i = 0; i < NTHREADS - 1; ++i)
		count += results[i];
	return count;
}

// task 6
volatile long volume_work_producer = ROWS;
volatile long volume_work_consumer = ROWS;
std::mutex mut;
std::condition_variable cv;
ThreadsafeQueue<int*> que_ptr_int;

void task_producer()
{
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<int> dist(0, 50); // 0..50, чтобы после *2 не выйти за 100

	int row_index{};
    while (_InterlockedExchangeAdd(&volume_work_producer, -1) > 0)
    {
		int* row = new int[COLS];
		for (size_t i{}; i < COLS; ++i)
		{
			if (row_index % 2 == 0) // чётные строки
				row[i] = dist(gen) * 2; // генерируем только чётные числа
			else
				row[i] = dist(gen); 
			std::cout << row[i] << ' ';
			std::this_thread::sleep_for(std::chrono::milliseconds(200));
		}
		std::cout << '\n';
		que_ptr_int.push(row);
		row_index++;
		cv.notify_all();
    }

	while (_InterlockedExchangeAdd(&volume_work_producer, -1) > 0)
	{
		int* row = new int[COLS];
		for (size_t i{}; i < COLS; ++i)
		{
			row[i] = dist(gen);
			std::cout << row[i] << ' ';
			std::this_thread::sleep_for(std::chrono::milliseconds(200));
		}
		std::cout << '\n';
		que_ptr_int.push(row);
		cv.notify_all();
	}
}

void task_consumer(int& count)
{
	count = 0;
	bool all_even{};

	while (_InterlockedExchangeAdd(&volume_work_consumer, -1) > 0)
	{
		int* row = nullptr;
		std::unique_lock<std::mutex> locker(mut);
		cv.wait_for(locker, std::chrono::seconds(5), []() {return !que_ptr_int.empty(); });
		if (que_ptr_int.try_pop(row))
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(400));
			all_even = is_all_even(row);
			std::cout << std::boolalpha << all_even << '\n';
			if (all_even)
				count++;
			delete[] row;
		}
		else
		{
			_InterlockedExchangeAdd(&volume_work_consumer, 1);
		}
	}
}

// task 7 (OpenMP)
int all_even_rows_count_parallel7(int** matrix)
{
	int global_cnt{};
#pragma omp parallel shared(matrix)
	{
		int local_cnt{};
#pragma omp for
		{
			for (size_t i = 0; i < ROWS; ++i)
			{
				bool result = true;
				int j{};
				while (j < COLS && result)
				{
					if (matrix[i][j] % 2 != 0)
						result = false;
					j++;
				}
				if (result)
					local_cnt++;
			}
		}
		#pragma omp critical
		{
			global_cnt += local_cnt;
		}
	}
	return global_cnt;
}



int main()
{
	srand(time(NULL));

	int** matrix = memory_allocation();
	std::ifstream file("data.txt");
	init_matrix(matrix, 0, 100);

	std::cout << "Matrix:\n";
	print_matrix(matrix);
	std::cout << "\nNon parallel solution\nCount of all-even rows = " << all_even_rows_count(matrix, 0, ROWS) << '\n';

	std::cout << "\nTask 1:\n";
	std::cout << "Parallel solution = " << all_even_rows_count_parallel(matrix) << '\n';

	std::cout << "\nTask 2:\n";
	std::cout << "Parallel solution = " << all_even_rows_count_parallel2(matrix) << '\n';

	std::cout << "\nTask 3:\n";
	std::cout << "Parallel solution = " << all_even_rows_count_parallel3(matrix) << '\n';

	std::cout << "\nTask 4:\n";
	std::cout << "Parallel solution = " << all_even_rows_count_parallel4(matrix) << '\n';

	std::cout << "\nTask 5:\n";
	std::cout << "Parallel solution = " << all_even_rows_count_parallel5(matrix) << '\n';

	std::cout << "\nTask 6:\n";

	int count{};
	std::thread workers[NTHREADS];
	for (int i{}; i < NTHREADS; ++i)
	{
		if (i < 2)
			workers[i] = std::thread(task_producer);
		else
			workers[i] = std::thread(task_consumer, std::ref(count));
	}

	for (int i = 0; i < NTHREADS; ++i)
		workers[i].join();
	std::cout << "Parallel solution = " << count << '\n';

	std::cout << "\nTask 7:\n";
	std::cout << "Parallel solution = " << all_even_rows_count_parallel7(matrix) << '\n';

	free_memory(matrix);

	return 0;
}