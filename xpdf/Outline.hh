//========================================================================
//
// Outline.h
//
// Copyright 2002-2013 Glyph & Cog, LLC
//
//========================================================================

#ifndef OUTLINE_H
#define OUTLINE_H

#include <config.hh>

#include <xpdf/Object.hh>
#include <xpdf/CharTypes.hh>

class GString;
class GList;
class XRef;
class LinkAction;
class TextString;

//------------------------------------------------------------------------

class Outline {
public:

  Outline(Object *outlineObj, XRef *xref);
  ~Outline();

  GList *getItems() { return items; }

private:

  GList *items;			// NULL if document has no outline
				//   [OutlineItem]
};

//------------------------------------------------------------------------

class OutlineItem {
public:

  OutlineItem(Object *itemRefA, Dict *dict, OutlineItem *parentA, XRef *xrefA);
  ~OutlineItem();

  static GList *readItemList(Object *firstItemRef, Object *lastItemRef,
			     OutlineItem *parentA, XRef *xrefA);

  void open();
  void close();

  Unicode *getTitle();
  int getTitleLength();
  TextString *getTitleTextString() { return title; }
  LinkAction *getAction() { return action; }
  GBool isOpen() { return startsOpen; }
  GBool hasKids() { return firstRef.isRef(); }
  GList *getKids() { return kids; }

private:

  XRef *xref;
  TextString *title;		// may be NULL
  LinkAction *action;
  Object itemRef;
  Object firstRef;
  Object lastRef;
  Object nextRef;
  GBool startsOpen;
  GList *kids;			// NULL unless this item is open [OutlineItem]
  OutlineItem *parent;
};

#endif
