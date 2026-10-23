"""Exercise the actual vendor remove callback with detached SDIO objects."""
from pathlib import Path
import subprocess,sys,tempfile

source=Path(sys.argv[1]).read_text()
start=source.index('static void aicbsp_sdio_remove(')
end=source.index('\n}\n',start)+3
callback=source[start:end]
prefix=r'''
#include <assert.h>
#include <stdlib.h>
#include <stdint.h>
#include <stddef.h>
enum { SYSTEM_RUNNING, SYSTEM_RESTART, SYSTEM_HALT, SYSTEM_POWER_OFF };
static int system_state = SYSTEM_RUNNING;
struct mmc_host { unsigned caps; };
struct mmc_card { struct mmc_host *host; };
struct device { void *data; };
struct sdio_func { struct mmc_card *card; struct device dev; };
struct aic_sdio_dev;
struct aicwf_bus { struct { struct aic_sdio_dev *sdio; } bus_priv; };
struct aic_sdio_dev { struct sdio_func *func; struct aicwf_bus *bus_if; };
static struct aic_sdio_dev *aicbsp_sdiodev;
static int releases, deinits;
#define sdio_dbg(...) ((void)0)
#define AICWFDBG(...) ((void)0)
#define MMC_CAP_NONREMOVABLE (1u<<8)
#define kfree free
static void *aicbsp_get_drvdata(void *unused) { (void)unused; return aicbsp_sdiodev->bus_if; }
static void aicwf_sdio_release(struct aic_sdio_dev *d) { assert(d); releases++; }
static void aicwf_sdio_func_deinit(struct aic_sdio_dev *d) { assert(d); deinits++; }
static void dev_set_drvdata(struct device *d,void *p) { d->data=p; }
'''
suffix=r'''
int main(void) {
    for (int state=SYSTEM_RESTART;state<=SYSTEM_POWER_OFF;state++) {
        system_state=state;
        /* Non-NULL freed sibling allocations must never be dereferenced. */
        aicbsp_sdiodev=(struct aic_sdio_dev *)(uintptr_t)0xdead1000;
        aicbsp_sdio_remove((struct sdio_func *)(uintptr_t)0xdead2000);
        assert(!aicbsp_sdiodev && releases==0 && deinits==0);
    }
    system_state=SYSTEM_RUNNING;
    struct mmc_host host={MMC_CAP_NONREMOVABLE|1};
    struct mmc_card card={&host};
    struct sdio_func first={&card,{NULL}},second={&card,{NULL}};
    aicbsp_sdio_remove(&second);
    for (int scenario=0;scenario<6;scenario++) {
        struct aic_sdio_dev *d=calloc(1,sizeof(*d));
        struct aicwf_bus *bus=calloc(1,sizeof(*bus));
        d->func=&first;d->bus_if=bus;bus->bus_priv.sdio=d;
        aicbsp_sdiodev=d;first.card=&card;card.host=&host;
        if(scenario==0)d->bus_if=NULL;
        if(scenario==1)d->func=NULL;
        if(scenario==2)first.card=NULL;
        if(scenario==3)card.host=NULL;
        if(scenario==4)bus->bus_priv.sdio=NULL;
        aicbsp_sdio_remove(&second);
        if(scenario==5) {
            assert(releases==1 && deinits==1);
            assert(!aicbsp_sdiodev);
            assert(!(host.caps&MMC_CAP_NONREMOVABLE));
            aicbsp_sdio_remove(&second);
            assert(releases==1 && deinits==1);
        } else {
            assert(releases==0 && deinits==0);
            if(scenario>=1 && scenario<=3)assert(!aicbsp_sdiodev);
            free(bus);free(d);aicbsp_sdiodev=NULL;
        }
    }
    return 0;
}
'''
with tempfile.TemporaryDirectory(prefix='aic-remove-validation-') as temp:
    c=Path(temp)/'remove.c';exe=Path(temp)/'remove'
    c.write_text(prefix+callback+suffix)
    subprocess.run(['gcc','-std=c11','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer',str(c),'-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
print('PASS: actual AIC remove under ASan/UBSan: reboot/halt/poweroff skip poisoned freed sibling handles; running-state empty/null cases, normal cleanup and duplicate callback')
