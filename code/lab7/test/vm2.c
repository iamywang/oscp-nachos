#include "syscall.h"

#define ARRAYSIZE 512

int A[ARRAYSIZE];

int main()
{
    int i, j, tmp;

    for (i = 0; i < ARRAYSIZE; i++)
        A[i] = ARRAYSIZE - i - 1;

    for (i = 0; i < (ARRAYSIZE - 1); i++)
        for (j = 0; j < ((ARRAYSIZE - 1) - i); j++)
            if (A[j] > A[j + 1])
            {
                tmp = A[j];
                A[j] = A[j + 1];
                A[j + 1] = tmp;
            }
    Exec("halt");
}