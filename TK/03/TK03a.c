#include <libavutil/avutil.h>
#include <stdio.h>

int main(void) {
    printf("libavutil version: %s\n", av_version_info());
    return 0;
}
