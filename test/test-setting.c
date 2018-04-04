#include <assert.h>
#include <string.h>

#include "../src/log.h"
#include "../src/common.h"
#include "../src/setting.h"

int main(int argc, char **argv)
{
    if (argc == 2 && !strncmp(argv[1], "-v", 2))
        yhttp_log_set(LOG_DEBUG2);

    int ret;
    ret = setting_init_default(&SETTING);
    assert(YHTTP_OK == ret);
    printf("Default settings\n");
    setting_dump(&SETTING);

    printf("Load from `conf.test`\n");
    ret = setting_parse("conf.test", &SETTING);
    assert(YHTTP_OK == ret);
    ret = setting_dump(&SETTING);
    assert(YHTTP_OK == ret);

    setting_destroy(&SETTING);
}
