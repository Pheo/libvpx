#include <stdio.h>
#include <emscripten.h>

EMSCRIPTEN_KEEPALIVE
int fibonacci(int n) {
    if (n <= 1)
        return n;
    return fibonacci(n - 1) + fibonacci(n - 2);
}

EMSCRIPTEN_KEEPALIVE
int main() {
    int n = 10; // Change the value of n as needed
    printf("Fibonacci sequence up to %d:\n", n);
    for (int i = 0; i < n; ++i) {
        printf("%d ", fibonacci(i));
    }
    printf("\n");
    return 0;
}
