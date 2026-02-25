/*-------------------------------------------------------------------------
 * et-testdrive: Minimal Hello World device kernel
 *-------------------------------------------------------------------------*/

#include <stdint.h>
#include "etsoc/isa/hart.h"
#include "etsoc/common/utils.h"

int64_t entry_point(void);

int64_t entry_point(void)
{
    et_printf("Hello World from hart %d\n", get_hart_id());
    return 0;
}
