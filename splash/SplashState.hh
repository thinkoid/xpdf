//========================================================================
//
// SplashState.h
//
// Copyright 2003-2013 Glyph & Cog, LLC
//
//========================================================================

#ifndef SPLASHSTATE_H
#define SPLASHSTATE_H

#include <config.hh>

#include <splash/SplashTypes.hh>

class SplashPattern;
class SplashScreen;
class SplashClip;
class SplashBitmap;
class SplashPath;

//------------------------------------------------------------------------
// line cap values
//------------------------------------------------------------------------

#define splashLineCapButt       0
#define splashLineCapRound      1
#define splashLineCapProjecting 2

//------------------------------------------------------------------------
// line join values
//------------------------------------------------------------------------

#define splashLineJoinMiter     0
#define splashLineJoinRound     1
#define splashLineJoinBevel     2

//------------------------------------------------------------------------
// SplashState
//------------------------------------------------------------------------

class SplashState {
public:

  // Create a new state object, initialized with default settings.
  SplashState(int width, int height, GBool vectorAntialias,
	      SplashScreenParams *screenParams);
  SplashState(int width, int height, GBool vectorAntialias,
	      SplashScreen *screenA);

  // Copy a state object.
  SplashState *copy() { return new SplashState(this); }

  ~SplashState();

  // Set the stroke pattern.  This does not copy <strokePatternA>.
  void setStrokePattern(SplashPattern *strokePatternA);

  // Set the fill pattern.  This does not copy <fillPatternA>.
  void setFillPattern(SplashPattern *fillPatternA);

  // Set the screen.  This does not copy <screenA>.
  void setScreen(SplashScreen *screenA);

  // Set the line dash pattern.  This copies the <lineDashA> array.
  void setLineDash(SplashCoord *lineDashA, int lineDashLengthA,
		   SplashCoord lineDashPhaseA);

  void clipResetToRect(SplashCoord x0, SplashCoord y0,
		       SplashCoord x1, SplashCoord y1);
  SplashError clipToRect(SplashCoord x0, SplashCoord y0,
			 SplashCoord x1, SplashCoord y1);
  SplashError clipToPath(SplashPath *path, GBool eo);

  // Set the soft mask bitmap.
  void setSoftMask(SplashBitmap *softMaskA);

  // Set the transfer function.
  void setTransfer(Guchar *red, Guchar *green, Guchar *blue, Guchar *gray);

private:

  SplashState(SplashState *state);

  SplashCoord matrix[6];
  SplashPattern *strokePattern;
  SplashPattern *fillPattern;
  SplashScreen *screen;
  SplashBlendFunc blendFunc;
  SplashCoord strokeAlpha;
  SplashCoord fillAlpha;
  SplashCoord lineWidth;
  int lineCap;
  int lineJoin;
  SplashCoord miterLimit;
  SplashCoord flatness;
  SplashCoord *lineDash;
  int lineDashLength;
  SplashCoord lineDashPhase;
  GBool strokeAdjust;
  SplashClip *clip;
  GBool clipIsShared;
  SplashBitmap *softMask;
  GBool deleteSoftMask;
  GBool inNonIsolatedGroup;
  GBool inKnockoutGroup;
  Guchar rgbTransferR[256],
         rgbTransferG[256],
         rgbTransferB[256];
  Guchar grayTransfer[256];
  Guchar cmykTransferC[256],
         cmykTransferM[256],
         cmykTransferY[256],
         cmykTransferK[256];
  Guint overprintMask;

  SplashState *next;		// used by Splash class

  friend class Splash;
};

#endif
