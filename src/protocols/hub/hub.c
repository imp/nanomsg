/*
    Copyright (c) 2015 Cyril Plisko. All rights reserved.

    Permission is hereby granted, free of charge, to any person obtaining a copy
    of this software and associated documentation files (the "Software"),
    to deal in the Software without restriction, including without limitation
    the rights to use, copy, modify, merge, publish, distribute, sublicense,
    and/or sell copies of the Software, and to permit persons to whom
    the Software is furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included
    in all copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
    THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
    FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
    IN THE SOFTWARE.
*/

#include "hub.h"
#include "xhub.h"

#include "../../nn.h"
#include "../../hub.h"

#include "../../utils/cont.h"
#include "../../utils/alloc.h"
#include "../../utils/err.h"
#include "../../utils/list.h"

struct nn_hub {
    struct nn_xhub xhub;
};

/*  Private functions. */
static void nn_hub_init (struct nn_hub *self,
    const struct nn_sockbase_vfptr *vfptr, void *hint);
static void nn_hub_term (struct nn_hub *self);

/*  Implementation of nn_sockbase's virtual functions. */
static void nn_hub_destroy (struct nn_sockbase *self);
static int nn_hub_send (struct nn_sockbase *self, struct nn_msg *msg);
static int nn_hub_recv (struct nn_sockbase *self, struct nn_msg *msg);
static const struct nn_sockbase_vfptr nn_hub_sockbase_vfptr = {
    NULL,
    nn_hub_destroy,
    nn_xhub_add,
    nn_xhub_rm,
    nn_xhub_in,
    nn_xhub_out,
    nn_xhub_events,
    nn_hub_send,
    nn_hub_recv,
    nn_xhub_setopt,
    nn_xhub_getopt
};

static void nn_hub_init (struct nn_hub *self,
    const struct nn_sockbase_vfptr *vfptr, void *hint)
{
    nn_xhub_init (&self->xhub, vfptr, hint);
}

static void nn_hub_term (struct nn_hub *self)
{
    nn_xhub_term (&self->xhub);
}

static void nn_hub_destroy (struct nn_sockbase *self)
{
    struct nn_hub *hub;

    hub = nn_cont (self, struct nn_hub, xhub.sockbase);

    nn_hub_term (hub);
    nn_free (hub);
}

static int nn_hub_send (struct nn_sockbase *self, struct nn_msg *msg)
{
    int rc;
    struct nn_hub *hub;

    hub = nn_cont (self, struct nn_hub, xhub.sockbase);

    /*  Check for malformed messages. */
    if (nn_chunkref_size (&msg->sphdr))
        return -EINVAL;

    /*  Send the message. */
    rc = nn_xhub_send (&hub->xhub.sockbase, msg);
    errnum_assert (rc == 0, -rc);

    return 0;
}

static int nn_hub_recv (struct nn_sockbase *self, struct nn_msg *msg)
{
    int rc;
    struct nn_hub *hub;

    hub = nn_cont (self, struct nn_hub, xhub.sockbase);

    /*  Get next message. */
    rc = nn_xhub_recv (&hub->xhub.sockbase, msg);
    if (nn_slow (rc == -EAGAIN))
        return -EAGAIN;
    errnum_assert (rc == 0, -rc);
    nn_assert (nn_chunkref_size (&msg->sphdr) == sizeof (uint64_t));

    /*  Discard the header. */
    nn_chunkref_term (&msg->sphdr);
    nn_chunkref_init (&msg->sphdr, 0);

    return 0;
}

static int nn_hub_create (void *hint, struct nn_sockbase **sockbase)
{
    struct nn_hub *self;

    self = nn_alloc (sizeof (struct nn_hub), "socket (hub)");
    alloc_assert (self);
    nn_hub_init (self, &nn_hub_sockbase_vfptr, hint);
    *sockbase = &self->xhub.sockbase;

    return 0;
}

static struct nn_socktype nn_hub_socktype_struct = {
    AF_SP,
    NN_HUB,
    0,
    nn_hub_create,
    nn_xhub_ispeer,
    NN_LIST_ITEM_INITIALIZER
};

struct nn_socktype *nn_hub_socktype = &nn_hub_socktype_struct;

