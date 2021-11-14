#include <iostream>
#include <fstream>
#include <cstdlib>
#include <cmath>

#include <mpi.h>

using namespace std;

void fillArray(int* arr, int size) 
{
	for (int i = 0; i < size; i++) 
	{
		arr[i] = i;
	}
}

void swap(int* first, int* second) 
{
	int temp = *first;
	*first = *second;
	*second = temp;
}

void shuffleArray(int* arr, int size) 
{
	for (int i = 0; i < size; i++) 
	{
		swap(&arr[i], &arr[rand() % (size - 1)]);
	}
}

void printArrayToFile(int* arr, int size, ofstream &file) 
{
	for (int i = 0; i < size; i++) 
	{
		file << arr[i] << " ";
		if (!((i + 1) % 15))
			file << endl;
	}
	file << endl << endl;
}

bool isSeq(int* arr, int size) 
{
	for (int i = 1; i < size; i++)
	{
		if (arr[i - 1] != (arr[i] + 1))
		{
			cout << arr[i] << " " << arr[i + 1] << endl;
			return false;
		}
	}
	return true;
}

void copyArr(int* target, int* src, int size) 
{
	for (int i = 0; i < size; i++) 
	{
		target[i] = src[i];
	}
}

int* getRankGroup(int* arr, int size, int rank, int digit) 
{
	int* group, groupSize = 0, targetDigit;
	for (int i = 0; i < size; i++) 
	{
		targetDigit = arr[i];
		int j = rank;
		while (j > 1)
		{
			targetDigit /= 10;
			j--;
		}
		if ((targetDigit % 10) == digit)
			groupSize++;
	}
	group = new int[groupSize + 1];
	group[0] = groupSize;
	for (int i = 0, k = 1; i < size; i++)
	{
		targetDigit = arr[i];
		int j = rank;
		while (j > 1)
		{
			targetDigit /= 10;
			j--;
		}
		if ((targetDigit % 10) == digit) 
		{
			group[k] = arr[i];
			k++;
		}	
	}
	return group;
}

int getMaxArrRank(int* arr, int size) 
{
	int rank = 0, max = 0;
	for (int i = 0; i < size; i++) 
	{
		if (arr[i] > max)
			max = arr[i];
	}
	while (max) 
	{
		rank++;
		max /= 10;
	}
	return rank;
}

int* radixSort(int* arr, int size)
{
	int* result = new int[size];
	int* tempResArr = new int[size];
	int* group;
	copyArr(tempResArr, arr, size);
	for (int i = 1; i <= getMaxArrRank(arr, size); i++)
	{
		int resultI = 0;
		for (int j = 9; j >= 0; j--)
		{
			group = getRankGroup(tempResArr, size, i, j);
			for (int k = 1; k <= group[0]; k++, resultI++)
			{
				result[resultI] = group[k];
			}
			delete[] group;
		}
		copyArr(tempResArr, result, size);
	}
	delete[] tempResArr;
	return result;
}

void radixSortMPI(int* arr, int size, int* resultArr, int threadId, int numOfThreads)
{
	int* temp = new int[size];
	int* group;
	if (threadId == 0)
		copyArr(temp, arr, size);
	for (int rank = 1; rank <= getMaxArrRank(arr, size); rank++)
	{
		MPI_Bcast(temp, size, MPI_INT, 0, MPI_COMM_WORLD);
		int resultI = 0;
		// Get data
		if (threadId == 0)
		{
			for (int j = 9; j >= 0; j--)
			{
				int* recvBuf = new int[size];
				MPI_Recv(recvBuf, size, MPI_INT, MPI_ANY_SOURCE, j,
					MPI_COMM_WORLD, MPI_STATUS_IGNORE);
				for (int k = 1; k <= recvBuf[0]; k++, resultI++)
				{
					resultArr[resultI] = recvBuf[k];
				}
				delete[] recvBuf;
			}
			copyArr(temp, resultArr, size);
		}
		// Evaluations
		else 
		{
			int step = ceil(10. / (numOfThreads - 1));
			int digit = (threadId - 1) * step;
			for (int j = digit; (j < (digit + step)) && (j < 10); j++) 
			{
				group = getRankGroup(temp, size, rank, j);
				MPI_Send(group, group[0] + 1, MPI_INT, 0, j, MPI_COMM_WORLD);
				delete[] group;
			}
		}
	}
	delete[] temp;
}

int main(int argc, char* argv[]) 
{
	int size, threadId, numOfThreads;
	int* arr, * resL, * resP;
	double startL, endL, startP, endP;
	double timeL, timeP;
	ofstream shuffled, sortedL, sortedP;
	if (argc > 1) 
	{
		size = atoi(argv[1]);
	}
	else 
	{
		cout << "Size isn't specified. Default size = 100" << endl;
		size = 100;
	}
	arr = new int[size];
	resP = new int[size];

	MPI_Init(&argc, &argv);
	MPI_Comm_size(MPI_COMM_WORLD, &numOfThreads);
	MPI_Comm_rank(MPI_COMM_WORLD, &threadId);
	if (threadId == 0)
	{
		shuffled.open("shuffled.txt"); sortedL.open("sortedL.txt");
		sortedP.open("sortedP.txt");
		fillArray(arr, size);
		shuffleArray(arr, size);
		printArrayToFile(arr, size, shuffled);
		startL = MPI_Wtime();
		resL = radixSort(arr, size);
		endL = MPI_Wtime();
		timeL = endL - startL;
		cout << "Linear sort: array is sorted - " << isSeq(resL, size) << " Time: " << timeL << endl;
		printArrayToFile(resL, size, sortedL);
	}

	MPI_Bcast(arr, size, MPI_INT, 0, MPI_COMM_WORLD);
	startP = MPI_Wtime();
	radixSortMPI(arr, size, resP, threadId, numOfThreads);
	endP = MPI_Wtime();
	timeP = endP - startP;

	if (threadId == 0)
	{
		cout << "Parallel sort: array is sorted - " << isSeq(resP, size) << " Time: " << timeP << endl;
		cout << "Parallel faster then linear on: " << timeL - timeP << " seconds" << endl;
		printArrayToFile(resP, size, sortedP);
		shuffled.close(); sortedL.close(); sortedP.close();
	}
	MPI_Finalize();
	return 0;
}