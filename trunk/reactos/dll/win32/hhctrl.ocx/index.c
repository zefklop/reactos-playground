/*
 * Copyright 2007 Jacek Caban for CodeWeavers
 * Copyright 2010 Erich Hoover
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */

#define NONAMELESSUNION
#define NONAMELESSSTRUCT

#include "hhctrl.h"
#include "stream.h"

#include <wine/debug.h>

WINE_DEFAULT_DEBUG_CHANNEL(htmlhelp);

/* Fill the TreeView object corresponding to the Index items */
static void fill_index_tree(HWND hwnd, IndexItem *item)
{
    int index = 0;
    LVITEMW lvi;

    while(item) {
        TRACE("tree debug: %s\n", debugstr_w(item->keyword));

        if(!item->keyword)
        {
            FIXME("HTML Help index item has no keyword.\n");
            item = item->next;
            continue;
        }
        memset(&lvi, 0, sizeof(lvi));
        lvi.iItem = index++;
        lvi.mask = LVIF_TEXT|LVIF_PARAM|LVIF_INDENT;
        lvi.iIndent = item->indentLevel;
        lvi.cchTextMax = strlenW(item->keyword)+1;
        lvi.pszText = item->keyword;
        lvi.lParam = (LPARAM)item;
        item->id = (HTREEITEM)SendMessageW(hwnd, LVM_INSERTITEMW, 0, (LPARAM)&lvi);
        item = item->next;
    }
}

/* Parse the attributes correspond to a list item, including sub-topics.
 *
 * Each list item has, at minimum, a param of type "keyword" and two
 * parameters corresponding to a "sub-topic."  For each sub-topic there
 * must be a "name" param and a "local" param, if there is only one
 * sub-topic then there isn't really a sub-topic, the index will jump
 * directly to the requested item.
 */
static void parse_index_obj_node_param(IndexItem *item, const char *text)
{
    const char *ptr;
    LPWSTR *param;
    int len, wlen;

    ptr = get_attr(text, "name", &len);
    if(!ptr) {
        WARN("name attr not found\n");
        return;
    }

    /* Allocate a new sub-item, either on the first run or whenever a
     * sub-topic has filled out both the "name" and "local" params.
     */
    if(item->itemFlags == 0x11 && (!strncasecmp("name", ptr, len) || !strncasecmp("local", ptr, len))) {
        item->nItems++;
        item->items = heap_realloc(item->items, sizeof(IndexSubItem)*item->nItems);
        item->items[item->nItems-1].name = NULL;
        item->items[item->nItems-1].local = NULL;
        item->itemFlags = 0x00;
    }
    if(!strncasecmp("keyword", ptr, len)) {
        param = &item->keyword;
    }else if(!item->keyword && !strncasecmp("name", ptr, len)) {
        /* Some HTML Help index files use an additional "name" parameter
         * rather than the "keyword" parameter.  In this case, the first
         * occurance of the "name" parameter is the keyword.
         */
        param = &item->keyword;
    }else if(!strncasecmp("name", ptr, len)) {
        item->itemFlags |= 0x01;
        param = &item->items[item->nItems-1].name;
    }else if(!strncasecmp("local", ptr, len)) {
        item->itemFlags |= 0x10;
        param = &item->items[item->nItems-1].local;
    }else {
        WARN("unhandled param %s\n", debugstr_an(ptr, len));
        return;
    }

    ptr = get_attr(text, "value", &len);
    if(!ptr) {
        WARN("value attr not found\n");
        return;
    }

    wlen = MultiByteToWideChar(CP_ACP, 0, ptr, len, NULL, 0);
    *param = heap_alloc((wlen+1)*sizeof(WCHAR));
    MultiByteToWideChar(CP_ACP, 0, ptr, len, *param, wlen);
    (*param)[wlen] = 0;
}

/* Parse the object tag corresponding to a list item.
 *
 * At this step we look for all of the "param" child tags, using this information
 * to build up the information about the list item.  When we reach the </object>
 * tag we know that we've finished parsing this list item.
 */
static IndexItem *parse_index_sitemap_object(HHInfo *info, stream_t *stream)
{
    strbuf_t node, node_name;
    IndexItem *item;

    strbuf_init(&node);
    strbuf_init(&node_name);

    item = heap_alloc_zero(sizeof(IndexItem));
    item->nItems = 0;
    item->items = heap_alloc_zero(0);
    item->itemFlags = 0x11;

    while(next_node(stream, &node)) {
        get_node_name(&node, &node_name);

        TRACE("%s\n", node.buf);

        if(!strcasecmp(node_name.buf, "param")) {
            parse_index_obj_node_param(item, node.buf);
        }else if(!strcasecmp(node_name.buf, "/object")) {
            break;
        }else {
            WARN("Unhandled tag! %s\n", node_name.buf);
        }

        strbuf_zero(&node);
    }

    strbuf_free(&node);
    strbuf_free(&node_name);

    return item;
}

/* Parse the HTML list item node corresponding to a specific help entry.
 *
 * At this stage we look for the only child tag we expect to find under
 * the list item: the <OBJECT> tag.  We also only expect to find object
 * tags with the "type" attribute set to "text/sitemap".
 */
static IndexItem *parse_li(HHInfo *info, stream_t *stream)
{
    strbuf_t node, node_name;
    IndexItem *ret = NULL;

    strbuf_init(&node);
    strbuf_init(&node_name);

    while(next_node(stream, &node)) {
        get_node_name(&node, &node_name);

        TRACE("%s\n", node.buf);

        if(!strcasecmp(node_name.buf, "object")) {
            const char *ptr;
            int len;

            static const char sz_text_sitemap[] = "text/sitemap";

            ptr = get_attr(node.buf, "type", &len);

            if(ptr && len == sizeof(sz_text_sitemap)-1
               && !memcmp(ptr, sz_text_sitemap, len)) {
                ret = parse_index_sitemap_object(info, stream);
                break;
            }
        }else {
            WARN("Unhandled tag! %s\n", node_name.buf);
        }

        strbuf_zero(&node);
    }

    strbuf_free(&node);
    strbuf_free(&node_name);

    return ret;
}

/* Parse the HTML Help page corresponding to all of the Index items.
 *
 * At this high-level stage we locate out each HTML list item tag.
 * Since there is no end-tag for the <LI> item, we must hope that
 * the <LI> entry is parsed correctly or tags might get lost.
 *
 * Within each entry it is also possible to encounter an additional
 * <UL> tag.  When this occurs the tag indicates that the topics
 * contained within it are related to the parent <LI> topic and
 * should be inset by an indent.
 */
static void parse_hhindex(HHInfo *info, IStream *str, IndexItem *item)
{
    stream_t stream;
    strbuf_t node, node_name;
    int indent_level = -1;

    strbuf_init(&node);
    strbuf_init(&node_name);

    stream_init(&stream, str);

    while(next_node(&stream, &node)) {
        get_node_name(&node, &node_name);

        TRACE("%s\n", node.buf);

        if(!strcasecmp(node_name.buf, "li")) {
            item->next = parse_li(info, &stream);
            item->next->merge = item->merge;
            item = item->next;
            item->indentLevel = indent_level;
        }else if(!strcasecmp(node_name.buf, "ul")) {
            indent_level++;
        }else if(!strcasecmp(node_name.buf, "/ul")) {
            indent_level--;
        }else {
            WARN("Unhandled tag! %s\n", node_name.buf);
        }

        strbuf_zero(&node);
    }

    strbuf_free(&node);
    strbuf_free(&node_name);
}

/* Initialize the HTML Help Index tab */
void InitIndex(HHInfo *info)
{
    IStream *stream;

    info->index = heap_alloc_zero(sizeof(IndexItem));
    info->index->nItems = 0;
    SetChmPath(&info->index->merge, info->pCHMInfo->szFile, info->WinType.pszIndex);

    stream = GetChmStream(info->pCHMInfo, info->pCHMInfo->szFile, &info->index->merge);
    if(!stream) {
        TRACE("Could not get index stream\n");
        return;
    }

    parse_hhindex(info, stream, info->index);
    IStream_Release(stream);

    fill_index_tree(info->tabs[TAB_INDEX].hwnd, info->index->next);
}

/* Free all of the Index items, including all of the "sub-items" that
 * correspond to different sub-topics.
 */
void ReleaseIndex(HHInfo *info)
{
    IndexItem *item = info->index, *next;
    int i;

    /* Note: item->merge is identical for all items, only free once */
    heap_free(item->merge.chm_file);
    heap_free(item->merge.chm_index);
    while(item) {
        next = item->next;

        heap_free(item->keyword);
        for(i=0;i<item->nItems;i++) {
            heap_free(item->items[i].name);
            heap_free(item->items[i].local);
        }
        heap_free(item->items);

        item = next;
    }
}
