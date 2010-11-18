/*
 *  This file is part of RawTherapee.
 *
 *  Copyright (c) 2004-2010 Gabor Horvath <hgabor@rawtherapee.com>
 *
 *  RawTherapee is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 * 
 *  RawTherapee is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with RawTherapee.  If not, see <http://www.gnu.org/licenses/>.
 */
#ifndef _IMPROCFUN_H_
#define _IMPROCFUN_H_

#include <image16.h>
#include <image8.h>
#include <procparams.h>
#include <shmap.h>
#include <coord2d.h>
#include <labimage.h>

namespace rtengine {

using namespace procparams;

class ImProcFunctions {

        static int* cacheL;
        static int* cachea;
        static int* cacheb;
        static int* xcache;
        static int* ycache;
        static int* zcache;
        static unsigned short gamma2curve[65536];

		cmsHTRANSFORM monitorTransform;

        int chroma_scale;
        int chroma_radius;

		const ProcParams* params;
		double scale;
		bool multiThread;

        void simpltransform     (Image16* original, Image16* transformed, int cx, int cy, int sx, int sy, int oW, int oH);
        void vignetting         (Image16* original, Image16* transformed, int cx, int cy, int oW, int oH);
        void transformNonSep    (Image16* original, Image16* transformed, int cx, int cy, int sx, int sy, int oW, int oH);
        void transformSep    	(Image16* original, Image16* transformed, int cx, int cy, int sx, int sy, int oW, int oH);
        void sharpenHaloCtrl    (LabImage* lab, unsigned short** blurmap, unsigned short** base, int W, int H);
        void firstAnalysis_     (Image16* original, Glib::ustring wprofile, unsigned int* histogram, int* chroma_radius, int row_from, int row_to);
        void dcdamping          (float** aI, unsigned short** aO, float damping, int W, int H);

        bool needsCA			();
        bool needsDistortion	();
        bool needsRotation		();
        bool needsPerspective	();
        bool needsVignetting	();


    public:

        double lumimul[3];

        static void initCache ();
        
        ImProcFunctions (const ProcParams* iparams, bool imultiThread=true)
			: monitorTransform(NULL), params(iparams), scale(1), multiThread(imultiThread) {}
        ~ImProcFunctions ();

        void setScale 		(double iscale);

        bool needsTransform ();

        void firstAnalysis  (Image16* working, const ProcParams* params, unsigned int* vhist16, double gamma);
        void rgbProc        (Image16* working, LabImage* lab, float* hltonecurve, float* shtonecurve, int* tonecurve, SHMap* shmap, int sat);
        void luminanceCurve (LabImage* lold, LabImage* lnew, int* curve, int row_from, int row_to);
		void chrominanceCurve (LabImage* lold, LabImage* lnew, int channel, int* curve, int row_from, int row_to);
        void colorCurve     (LabImage* lold, LabImage* lnew);
        void sharpening     (LabImage* lab, unsigned short** buffer);
        void lumadenoise    (LabImage* lab, int** buffer);
        void colordenoise   (LabImage* lab, int** buffer);
        void transform      (Image16* original, Image16* transformed, int cx, int cy, int sx, int sy, int oW, int oH);
        void lab2rgb        (LabImage* lab, Image8* image);
        void resize         (Image16* src, Image16* dst);
        void deconvsharpening(LabImage* lab, unsigned short** buffer);
        void waveletEqualizer(Image16 * image);
        void waveletEqualizer(LabImage * image, bool luminance, bool chromaticity);
	
	void impulsedenoise (LabImage* lab);//Emil's impulse denoise
	void dirpyrdenoise (LabImage* lab);//Emil's impulse denoise
	void dirpyrequalizer (LabImage* lab);//Emil's equalizer

	void dirpyrLab_denoise(LabImage * src, LabImage * dst, int luma, int chroma, float gamma );//Emil's directional pyramid denoise
	void dirpyr(LabImage* data_fine, LabImage* data_coarse, int level, int * rangefn_L, int * rangefn_ab, int pitch, int scale, const int luma, int chroma );
	void idirpyr(LabImage* data_coarse, LabImage* data_fine, int level, float * nrwt_l, float * nrwt_ab, int pitch, int scale, const int luma, int chroma );
		
	void dirpyrLab_equalizer(LabImage * src, LabImage * dst, const double * mult );//Emil's directional pyramid equalizer
	void dirpyr_eq(LabImage* data_coarse, LabImage* data_fine, int * rangefn, int level, int pitch, int scale, const double * mult );
	void idirpyr_eq(LabImage* data_coarse, LabImage* data_fine, int *** buffer, int * irangefn, int level, int pitch, int scale, const double * mult );
	
	void dirpyr_equalizer(unsigned short ** src, unsigned short ** dst, int srcwidth, int srcheight, const double * mult );//Emil's directional pyramid equalizer
	void dirpyr_channel(unsigned short ** data_fine, unsigned short ** data_coarse, int width, int height, int * rangefn, int level, int scale, const double * mult  );
	void idirpyr_eq_channel(unsigned short ** data_coarse, unsigned short ** data_fine, int ** buffer, int width, int height, int level, const double * mult );
	
        Image8*     lab2rgb     (LabImage* lab, int cx, int cy, int cw, int ch, Glib::ustring profile);
        Image16*    lab2rgb16   (LabImage* lab, int cx, int cy, int cw, int ch, Glib::ustring profile);

        bool transCoord     (int W, int H, int x, int y, int w, int h, int& xv, int& yv, int& wv, int& hv, double ascaleDef = -1);
        bool transCoord     (int W, int H, std::vector<Coord2D> &src, std::vector<Coord2D> &red,  std::vector<Coord2D> &green, std::vector<Coord2D> &blue, double ascaleDef = -1);
        void getAutoExp  	(unsigned int* histogram, int histcompr, double expcomp, double clip, double& br, int& bl);
        double getTransformAutoFill (int oW, int oH);
	
	void rgb2hsv (int r, int g, int b, float &h, float &s, float &v); 
	void hsv2rgb (float h, float s, float v, int &r, int &g, int &b); 
};
}
#endif