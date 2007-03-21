/* $Id:$ */

/*
 * Copyright (c) 2006-2007 Aaron Turner.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the names of the copyright owners nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
 * IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "dlt_plugins-int.h"
#include "dlt_utils.h"
#include "hdlc.h"
#include "tcpedit.h"
#include "common.h"
#include "tcpr.h"

static char dlt_name[] = "hdlc";
static char __attribute__((unused)) dlt_prefix[] = "hdlc";
static u_int16_t dlt_value = DLT_C_HDLC;

/*
 * Function to register ourselves.  This function is always called, regardless
 * of what DLT types are being used, so it shouldn't be allocating extra buffers
 * or anything like that (use the dlt_hdlc_init() function below for that).
 * Tasks:
 * - Create a new plugin struct
 * - Fill out the provides/requires bit masks.  Note:  Only specify which fields are
 *   actually in the header.
 * - Add the plugin to the context's plugin chain
 * Returns: TCPEDIT_ERROR | TCPEDIT_OK | TCPEDIT_WARN
 */
int 
dlt_hdlc_register(tcpeditdlt_t *ctx)
{
    tcpeditdlt_plugin_t *plugin;
    assert(ctx);

    /* create  a new plugin structure */
    plugin = tcpedit_dlt_newplugin();

    /* FIXME: set what we provide & require  */
    plugin->provides += PLUGIN_MASK_PROTO;
    plugin->requires += PLUGIN_MASK_PROTO;

     /* what is our DLT value? */
    plugin->dlt = dlt_value;

    /* set the prefix name of our plugin.  This is also used as the prefix for our options */
    plugin->name = safe_strdup(dlt_prefix);

    /* 
     * Point to our functions, note, you need a function for EVERY method.  
     * Even if it is only an empty stub returning success.
     */
    plugin->plugin_init = dlt_hdlc_init;
    plugin->plugin_cleanup = dlt_hdlc_cleanup;
    plugin->plugin_parse_opts = dlt_hdlc_parse_opts;
    plugin->plugin_decode = dlt_hdlc_decode;
    plugin->plugin_encode = dlt_hdlc_encode;
    plugin->plugin_proto = dlt_hdlc_proto;
    plugin->plugin_l2addr_type = dlt_hdlc_l2addr_type;
    plugin->plugin_l2len = dlt_hdlc_l2len;
    plugin->plugin_get_layer3 = dlt_hdlc_get_layer3;
    plugin->plugin_merge_layer3 = dlt_hdlc_merge_layer3;

    /* add it to the available plugin list */
    return tcpedit_dlt_addplugin(ctx, plugin);
}

 
/*
 * Initializer function.  This function is called only once, if and only iif
 * this plugin will be utilized.  Remember, if you need to keep track of any state, 
 * store it in your plugin->config, not a global!
 * Returns: TCPEDIT_ERROR | TCPEDIT_OK | TCPEDIT_WARN
 */
int 
dlt_hdlc_init(tcpeditdlt_t *ctx)
{
    tcpeditdlt_plugin_t *plugin;
    hdlc_config_t *config;
    assert(ctx);
    
    if ((plugin = tcpedit_dlt_getplugin(ctx, dlt_value)) == NULL) {
        tcpedit_seterr(ctx->tcpedit, "Unable to initalize unregistered plugin %s", dlt_name);
        return TCPEDIT_ERROR;
    }
    
    /* allocate memory for our deocde extra data */
    if (sizeof(hdlc_extra_t) > 0)
        ctx->decoded_extra = safe_malloc(sizeof(hdlc_extra_t));

    /* allocate memory for our config data */
    if (sizeof(hdlc_config_t) > 0)
        plugin->config = safe_malloc(sizeof(hdlc_config_t));

    config = (hdlc_config_t *)plugin->config;

    /* default to unset */
    config->address = 65535;
    config->control = 65535;
    return TCPEDIT_OK; /* success */
}

/*
 * Since this is used in a library, we should manually clean up after ourselves
 * Unless you allocated some memory in dlt_hdlc_init(), this is just an stub.
 * Returns: TCPEDIT_ERROR | TCPEDIT_OK | TCPEDIT_WARN
 */
int 
dlt_hdlc_cleanup(tcpeditdlt_t *ctx)
{
    tcpeditdlt_plugin_t *plugin;
    assert(ctx);

    if ((plugin = tcpedit_dlt_getplugin(ctx, dlt_value)) == NULL) {
        tcpedit_seterr(ctx->tcpedit, "Unable to cleanup unregistered plugin %s", dlt_name);
        return TCPEDIT_ERROR;
    }

    if (ctx->decoded_extra != NULL) {
        free(ctx->decoded_extra);
        ctx->decoded_extra = NULL;
    }
        
    if (plugin->config != NULL) {
        free(plugin->config);
        plugin->config = NULL;
    }

    return TCPEDIT_OK; /* success */
}

/*
 * This is where you should define all your AutoGen AutoOpts option parsing.
 * Any user specified option should have it's bit turned on in the 'provides'
 * bit mask.
 * Returns: TCPEDIT_ERROR | TCPEDIT_OK | TCPEDIT_WARN
 */
int 
dlt_hdlc_parse_opts(tcpeditdlt_t *ctx)
{
    tcpeditdlt_plugin_t *plugin;
    hdlc_config_t *config;
    assert(ctx);

    plugin = tcpedit_dlt_getplugin(ctx, dlt_value);
    config = plugin->config;
    
    if (HAVE_OPT(HDLC_CONTROL)) {
        config->control = (u_int16_t)OPT_VALUE_HDLC_CONTROL;
    }
    
    if (HAVE_OPT(HDLC_ADDRESS)) {
        config->address = (u_int16_t)OPT_VALUE_HDLC_ADDRESS;
    }
    
    return TCPEDIT_OK; /* success */
}

/*
 * Function to decode the layer 2 header in the packet.
 * You need to fill out:
 * - ctx->l2len
 * - ctx->srcaddr
 * - ctx->dstaddr
 * - ctx->proto
 * - ctx->decoded_extra
 * Returns: TCPEDIT_ERROR | TCPEDIT_OK | TCPEDIT_WARN
 */
int 
dlt_hdlc_decode(tcpeditdlt_t *ctx, const u_char *packet, const int pktlen)
{
    cisco_hdlc_t *hdlc;
    hdlc_extra_t *extra;
    assert(ctx);
    assert(packet);
    assert(pktlen >= 4);
    
    extra = (hdlc_extra_t *)ctx->decoded_extra;
    hdlc = (cisco_hdlc_t *)packet;
    
    ctx->proto = hdlc->protocol;
    ctx->l2len = 4;
    
    extra->address = hdlc->address;
    extra->control = hdlc->control;
    
    return TCPEDIT_OK; /* success */
}

/*
 * Function to encode the layer 2 header back into the packet.
 * Returns: total packet len or TCPEDIT_ERROR
 */
int 
dlt_hdlc_encode(tcpeditdlt_t *ctx, u_char **packet_ex, int pktlen, __attribute__((unused)) tcpr_dir_t dir)
{
    cisco_hdlc_t *hdlc;
    hdlc_config_t *config = NULL;
    hdlc_extra_t *extra = NULL;
    tcpeditdlt_plugin_t *plugin = NULL;
    u_char *packet;
    int l2len;
    u_char tmpbuff[MAXPACKET];

    assert(ctx);
    assert(packet_ex);
    assert(pktlen >= 4);
    
    packet = *packet_ex;
    assert(packet);

    /* Make room for our new l2 header if l2len != 4 */
    if (l2len > 4) {
        memmove(packet + 4, packet + ctx->l2len, pktlen - ctx->l2len);
    } else if (l2len < 4) {
        memcpy(tmpbuff, packet, pktlen);
        memcpy(packet + 4, (tmpbuff + ctx->l2len), pktlen - ctx->l2len);
    }
    
    /* update the total packet length */
    pktlen += 4 - ctx->l2len;
    
    /* 
     * HDLC doesn't support direction, since we have no real src/dst addresses
     * to deal with, so we just use the original packet data or option data
     */
    hdlc = (cisco_hdlc_t *)packet;
    plugin = tcpedit_dlt_getplugin(ctx, dlt_value);
    config = plugin->config;
    extra = (hdlc_extra_t *)ctx->decoded_extra;

    /* set the address field */
    if (config->address < 65535) {
        hdlc->address = (u_int8_t)config->address;
    } else if (extra->hdlc) {
        hdlc->address = extra->hdlc;
    } else {
        tcpedit_seterr(ctx->tcpedit, "%s", "Non-HDLC packet requires --hdlc-address");
        return TCPEDIT_ERROR;
    }
    
    /* set the control field */
    if (config->control < 65535) {
        hdlc->control = (u_int8_t)config->control;
    } else if (extra->hdlc) {
        hdlc->control = extra->hdlc;
    } else {
        tcpedit_seterr(ctx->tcpedit, "%s", "Non-HDLC packet requires --hdlc-control");
        return TCPEDIT_ERROR;      
    }
    
    /* copy over our protocol */
    hdlc->protocol = ctx->proto;
    
    return pktlen; /* success */
}

/*
 * Function returns the Layer 3 protocol type of the given packet, or TCPEDIT_ERROR on error
 */
int 
dlt_hdlc_proto(tcpeditdlt_t *ctx, const u_char *packet, const int pktlen)
{
    cisco_hdlc_t *hdlc;
    assert(ctx);
    assert(packet);
    assert(pktlen >= 4);
    
    hdlc = (cisco_hdlc_t *)packet;
    
    return ntohs(hdlc->protocol);
}

/*
 * Function returns a pointer to the layer 3 protocol header or NULL on error
 */
u_char *
dlt_hdlc_get_layer3(tcpeditdlt_t *ctx, u_char *packet, const int pktlen)
{
    int l2len;
    assert(ctx);
    assert(packet);

    /* FIXME: Is there anything else we need to do?? */
    l2len = dlt_hdlc_l2len(ctx, packet, pktlen);

    assert(pktlen >= l2len);

    return tcpedit_dlt_l3data_copy(ctx, packet, pktlen, l2len);
}

/*
 * function merges the packet (containing L2 and old L3) with the l3data buffer
 * containing the new l3 data.  Note, if L2 % 4 == 0, then they're pointing to the
 * same buffer, otherwise there was a memcpy involved on strictly aligned architectures
 * like SPARC
 */
u_char *
dlt_hdlc_merge_layer3(tcpeditdlt_t *ctx, u_char *packet, const int pktlen, u_char *l3data)
{
    int l2len;
    assert(ctx);
    assert(packet);
    assert(l3data);
    
    /* FIXME: Is there anything else we need to do?? */
    l2len = dlt_hdlc_l2len(ctx, packet, pktlen);
    
    assert(pktlen >= l2len);
    
    return tcpedit_dlt_l3data_merge(ctx, packet, pktlen, l3data, l2len);
}

/* 
 * return the length of the L2 header of the current packet
 */
int
dlt_hdlc_l2len(tcpeditdlt_t *ctx, const u_char *packet, const int pktlen)
{
    assert(ctx);
    assert(packet);
    assert(pktlen);

    /* HDLC is a static 4 bytes */
    return 4;
}


tcpeditdlt_l2addr_type_t 
dlt_hdlc_l2addr_type(void)
{
    return C_HDLC;
}