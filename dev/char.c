/*
 * Copyright (C) 2014 F4OS Authors
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is furnished to do
 * so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <linker_array.h>
#include <stdlib.h>
#include <dev/char.h>
#include <dev/device.h>
#include <kernel/fault.h>
#include <kernel/obj.h>

LINKER_ARRAY_DECLARE(char_conversions)

static void char_dtor(struct obj *o);

static struct obj_type char_type_s  = {
    .offset = offset_of(struct char_device, obj),
    .dtor = char_dtor,
};

/*
 * Cleanup internal structures and put the base obj before destroying it, but
 * do not explicitly destroy or deinitialize it, as there may be other users.
 */
static void char_dtor(struct obj *o) {
    struct char_device *c;
    struct char_ops *ops;

    assert_type(o, &char_type_s);
    c = to_char_device(o);
    ops = (struct char_ops *)o->ops;

    ops->_cleanup(c);

    if (c->base) {
        obj_put(c->base);
    }

    free(c);
}

struct char_device *char_device_create(struct obj *base,
                                       struct char_ops *ops) {
    struct char_device *c = malloc(sizeof(*c));
    if (!c) {
        return NULL;
    }

    obj_init(&c->obj, &char_type_s, "Character device");
    c->obj.ops = ops;
    c->base = base;

    if (base) {
        obj_get(base);
    }

    return c;
}

struct char_device *char_device_cast(struct obj *base) {
    struct char_conversion *converter;

    if (!base) {
        return NULL;
    }

    /* Already a char_device! */
    if (base->type == &char_type_s) {
        obj_get(base);
        return to_char_device(base);
    }

    LINKER_ARRAY_FOR_EACH(char_conversions, converter) {
        if (converter->type == base->type) {
            return converter->cast(base);
        }
    }

    return NULL;
}

struct char_device *char_device_get(const char *name) {
    struct obj *base;
    struct char_device *dev;

    base = device_get(name);
    if (!base) {
        return NULL;
    }

    dev = char_device_cast(base);

    /*
     * char_device maintains dedicated reference to base obj,
     * ours is no longer necessary.  If char_device_cast() failed,
     * we also want to put the base obj to cleanup.
     */
    obj_put(base);

    return dev;
}

int char_device_base_equal(const struct char_device *d1,
                           const struct char_device *d2) {
    /* If there is no base pointer, we can't tell if they are equal */
    if (!d1->base || !d2->base) {
        return 0;
    }

    return d1->base == d2->base;
}

void init_io(void) {
    curr_task->_stdout = char_device_get(CONFIG_STDOUT_DEV);
    curr_task->_stdin =  curr_task->_stdout;
    curr_task->_stderr = char_device_get(CONFIG_STDERR_DEV);

    if (!curr_task->_stdout || !curr_task->_stderr) {
        panic();
    }
}
