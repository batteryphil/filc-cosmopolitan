#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void trigger_stack_overflow() {
    printf("[*] Attempting Stack Buffer Overflow...\n");
    char buffer[10];
    strcpy(buffer, "This string is intentionally way too long for a 10 byte buffer and should trigger fil-c bounds checking!");
    printf("    FAILED: Stack overflow bypassed memory safety: %s\n", buffer);
}

void trigger_heap_overflow() {
    printf("[*] Attempting Heap Buffer Overflow...\n");
    char *heap_buf = (char *)malloc(10);
    strcpy(heap_buf, "This is another intentionally massive string meant to overflow the heap.");
    printf("    FAILED: Heap overflow bypassed memory safety: %s\n", heap_buf);
    free(heap_buf);
}

void trigger_use_after_free() {
    printf("[*] Attempting Use-After-Free (UAF)...\n");
    char *uaf_buf = (char *)malloc(20);
    strcpy(uaf_buf, "Safe string");
    free(uaf_buf);
    
    // Attempting to write to freed memory
    strcpy(uaf_buf, "Dangerous write");
    printf("    FAILED: UAF bypassed memory safety: %s\n", uaf_buf);
}

void trigger_double_free() {
    printf("[*] Attempting Double Free...\n");
    char *df_buf = (char *)malloc(20);
    free(df_buf);
    free(df_buf); // Should trigger abort
    printf("    FAILED: Double Free bypassed memory safety!\n");
}

int main(int argc, char **argv) {
    printf("==========================================\n");
    printf("   Memory-Safe Cosmopolitan Stress Test   \n");
    printf("==========================================\n");
    
    if (argc < 2) {
        printf("Usage: ./stress_test.com [1|2|3|4]\n");
        printf("  1: Stack Overflow\n");
        printf("  2: Heap Overflow\n");
        printf("  3: Use-After-Free\n");
        printf("  4: Double Free\n");
        return 0;
    }

    int choice = atoi(argv[1]);

    switch (choice) {
        case 1: trigger_stack_overflow(); break;
        case 2: trigger_heap_overflow(); break;
        case 3: trigger_use_after_free(); break;
        case 4: trigger_double_free(); break;
        default: printf("Invalid choice.\n");
    }

    return 0;
}
