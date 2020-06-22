// -*- mode: c++; -*-
// Copyright 2002-2013 Glyph & Cog, LLC

#include <defs.hh>

#include <utils/string.hh>
#include <utils/GList.hh>

#include <xpdf/dict.hh>
#include <xpdf/Error.hh>
#include <xpdf/Link.hh>
#include <xpdf/Outline.hh>
#include <xpdf/TextString.hh>

//------------------------------------------------------------------------

Outline::Outline(Object *outlineObj, XRef *xref)
{
    Object first, last;

    items = NULL;
    if (!outlineObj->is_dict()) {
        return;
    }
    first = (*outlineObj).as_dict()["First"];
    last = (*outlineObj).as_dict()["Last"];
    if (first.is_ref() && last.is_ref()) {
        items = OutlineItem::readItemList(&first, &last, NULL, xref);
    }
}

Outline::~Outline()
{
    if (items) {
        deleteGList(items, OutlineItem);
    }
}

//------------------------------------------------------------------------

OutlineItem::OutlineItem(Object *itemRefA, Dict *dict, OutlineItem *parentA,
                         XRef *xrefA)
{
    Object obj1;

    xref = xrefA;
    title = NULL;
    action = NULL;
    kids = NULL;
    parent = parentA;

    if ((obj1 = resolve((*dict)["Title"])).is_string()) {
        title = new TextString(obj1.as_string());
    }

    if (!(obj1 = resolve((*dict)["Dest"])).is_null()) {
        action = LinkAction::parseDest(&obj1);
    } else {
        if (!(obj1 = resolve((*dict)["A"])).is_null()) {
            action = LinkAction::parseAction(&obj1);
        }
    }

    itemRef = *itemRefA;

    firstRef = (*dict)["First"];
    lastRef = (*dict)["Last"];
    nextRef = (*dict)["Next"];

    startsOpen = false;
    if ((obj1 = resolve((*dict)["Count"])).is_int()) {
        if (obj1.as_int() > 0) {
            startsOpen = true;
        }
    }
}

OutlineItem::~OutlineItem()
{
    close();
    if (title) {
        delete title;
    }
    if (action) {
        delete action;
    }
}

GList *OutlineItem::readItemList(Object *firstItemRef, Object *lastItemRef,
                                 OutlineItem *parentA, XRef *xrefA)
{
    GList *      items;
    OutlineItem *item, *sibling;
    Object       obj;
    Object *     p;
    OutlineItem *ancestor;
    int          i;

    items = new GList();

    if (!firstItemRef->is_ref() || !lastItemRef->is_ref()) {
        return items;
    }

    p = firstItemRef;

    do {
        if (!(obj = resolve(*p)).is_dict()) {
            break;
        }

        item = new OutlineItem(p, &obj.as_dict(), parentA, xrefA);

        // check for loops with parents
        for (ancestor = parentA; ancestor; ancestor = ancestor->parent) {
            if (p->getRefNum() == ancestor->itemRef.getRefNum() &&
                p->getRefGen() == ancestor->itemRef.getRefGen()) {
                error(errSyntaxError, -1, "Loop detected in outline");
                break;
            }
        }
        if (ancestor) {
            delete item;
            break;
        }

        // check for loops with siblings
        for (i = 0; i < items->getLength(); ++i) {
            sibling = (OutlineItem *)items->get(i);
            if (p->getRefNum() == sibling->itemRef.getRefNum() &&
                p->getRefGen() == sibling->itemRef.getRefGen()) {
                error(errSyntaxError, -1, "Loop detected in outline");
                break;
            }
        }
        if (i < items->getLength()) {
            delete item;
            break;
        }

        items->append(item);
        if (p->getRefNum() == lastItemRef->as_ref().num &&
            p->getRefGen() == lastItemRef->as_ref().gen) {
            break;
        }
        p = &item->nextRef;
        if (!p->is_ref()) {
            break;
        }
    } while (p);
    return items;
}

void OutlineItem::open()
{
    if (!kids) {
        kids = readItemList(&firstRef, &lastRef, this, xref);
    }
}

void OutlineItem::close()
{
    if (kids) {
        deleteGList(kids, OutlineItem);
        kids = NULL;
    }
}

Unicode *OutlineItem::getTitle()
{
    return title ? title->getUnicode() : (Unicode *)NULL;
}

int OutlineItem::getTitleLength()
{
    return title ? title->getLength() : 0;
}
