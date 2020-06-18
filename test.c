#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static char* word = "banana";

char *ltrim(char *str, const char *seps)
{
    size_t totrim;
    if (seps == NULL) {
        seps = "\t\n\v\f\r ";
    }
    totrim = strspn(str, seps);
    if (totrim > 0) {
        size_t len = strlen(str);
        if (totrim == len) {
            str[0] = '\0';
        }
        else {
            memmove(str, str + totrim, len + 1 - totrim);
        }
    }
    return str;
}

char *rtrim(char *str, const char *seps)
{
    int i;
    if (seps == NULL) {
        seps = "\t\n\v\f\r ";
    }
    i = strlen(str) - 1;
    while (i >= 0 && strchr(seps, str[i]) != NULL) {
        str[i] = '\0';
        i--;
    }
    return str;
}

char *trim(char *str, const char *seps)
{
    return ltrim(rtrim(str, seps), seps);
}

int main(void){
    char buffer[2080] = {};
    char * ptr = malloc(sizeof(char) * 2048);
    sprintf(buffer, "%s: %s", "haen","banana"); 
    strcpy(ptr, buffer);

    char* ptr_word_slice = strtok(ptr, ":");
    ptr_word_slice = strtok(NULL, ":");
                
    char * temp = trim(ptr_word_slice, NULL); 
    
    printf("%s %s %d %d", ptr_word_slice, temp, strcmp(temp, ptr_word_slice), strcmp(ptr_word_slice, word));
    return 0;
}