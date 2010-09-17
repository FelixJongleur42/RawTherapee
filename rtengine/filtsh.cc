/*
 * filtsh.cc
 *
 *  Created on: Sep 16, 2010
 *      Author: gabor
 */

#include "filtsh.h"
#include "rtengine.h"
#include "macros.h"
#include "improcfuns.h"
#include "buffer.h"
#include "gauss.h"
#include "bilateral2.h"

namespace rtengine {

class PreShadowsHighlightsFilterDescriptor : public FilterDescriptor {
    public:
    PreShadowsHighlightsFilterDescriptor ()
            : FilterDescriptor ("PreShadowsHighlights", MultiImage::RGB, MultiImage::RGB) {
            addTriggerEvent (EvSHEnabled);
            addTriggerEvent (EvSHRadius);
        }
        void createAndAddToList (Filter* tail) const {}
};

PreShadowsHighlightsFilterDescriptor preShadowsHighlightsFilterDescriptor;
ShadowsHighlightsFilterDescriptor shadowsHighlightsFilterDescriptor;

ShadowsHighlightsFilterDescriptor::ShadowsHighlightsFilterDescriptor ()
	: FilterDescriptor ("ShadowsHighlights", MultiImage::RGB, MultiImage::RGB) {

    addTriggerEvent (EvSHEnabled);
	addTriggerEvent (EvSHHighlights);
    addTriggerEvent (EvSHShadows);
    addTriggerEvent (EvSHHLTonalW);
    addTriggerEvent (EvSHSHTonalW);
    addTriggerEvent (EvSHLContrast);
}

void ShadowsHighlightsFilterDescriptor::createAndAddToList (Filter* tail) const {

    PreShadowsHighlightsFilter* pshf = new PreShadowsHighlightsFilter ();
    tail->addNext (pshf);
    tail->addNext (new ShadowsHighlightsFilter (pshf));
}

PreShadowsHighlightsFilter::PreShadowsHighlightsFilter ()
    : Filter (&preShadowsHighlightsFilterDescriptor), map (NULL) {
}

PreShadowsHighlightsFilter::~PreShadowsHighlightsFilter () {

    delete map;
}

void PreShadowsHighlightsFilter::process (const std::set<ProcEvent>& events, MultiImage* sourceImage, MultiImage* targetImage, Buffer<int>* buffer) {

    if (procParams->sh.enabled) {

        // calculate radius (procparams contains relative radius only)
        ImageSource* imgsrc = getFilterChain ()->getImageSource ();
        int pW, pH;
        imgsrc->getFullImageSize (pW, pH);
        double radius = sqrt (double(pW*pW+pH*pH)) / 2.0;
        double shradius = radius / 1800.0 * procParams->sh.radius;

        // allocate map
        delete map;
        map = new Buffer<unsigned short> (sourceImage->width, sourceImage->height);

        // fill with luminance
        TMatrix wprof = iccStore.workingSpaceMatrix (procParams->icm.working);
        double lumimulr = wprof[0][1];
        double lumimulg = wprof[1][1];
        double lumimulb = wprof[2][1];

        for (int i=0; i<map->height; i++)
            for (int j=0; j<map->width; j++) {
                int val = lumimulr*sourceImage->r[i][j] + lumimulg*sourceImage->g[i][j] + lumimulb*sourceImage->b[i][j];
                map->rows[i][j] = CLIP(val);
            }

        if (!procParams->sh.hq) {
            gaussHorizontal<unsigned short> (map, map, (double*)(buffer->data), radius, multiThread);
            gaussVertical<unsigned short>   (map, map, (double*)(buffer->data), radius, multiThread);
        }
        else {
            #pragma omp parallel if (multiThread)
            {
                int tid = omp_get_thread_num();
                int nthreads = omp_get_num_threads();
                int blk = H/nthreads;

                if (tid<nthreads-1)
                    bilateral<unsigned short> (map, map, W, H, 8000, radius, tid*blk, (tid+1)*blk);
                else
                    bilateral<unsigned short> (map, map, W, H, 8000, radius, tid*blk, H);
            }
            // anti-alias filtering the result
            for (int i=0; i<map->height; i++)
                for (int j=0; j<map->width; j++)
                    if (i>0 && j>0 && i<H-1 && j<W-1)
                        buffer->rows[i][j] = (map->rows[i-1][j-1]+map->rows[i-1][j]+map->rows[i-1][j+1]+map->rows[i][j-1]+map->rows[i][j]+map->rows[i][j+1]+map->rows[i+1][j-1]+map->rows[i+1][j]+map->rows[i+1][j+1])/9;
                    else
                        buffer->rows[i][j] = map->rows[i][j];
            for (int i=0; i<map->height; i++)
                for (int j=0; j<map->width; j++)
                    map->rows[i][j] = buffer->rows[i][j];
        }

        // update average, minimum, maximum
        Filter* p = getParentFilter ();
        if (!p) {
            double _avg = 0;
            int n = 1;
            min = 65535;
            max = 0;
            for (int i=32; i<H-32; i++)
                for (int j=32; j<W-32; j++) {
                    int val = map[i][j];
                    if (val < min)
                        min = val;
                    if (val > max)
                        max = val;
                    _avg = 1.0/n * val + (1.0 - 1.0/n) * _avg;
                    n++;
                }
            avg = (int) _avg;
        }
        else {
            Filter* root = p;
            while (root->getParentFilter())
                root = root->getParentFilter();
            min = ((PreShadowsHighlightsFilter*)root)->min;
            max = ((PreShadowsHighlightsFilter*)root)->max;
            avg = ((PreShadowsHighlightsFilter*)root)->avg;
        }
    }
    else {
        delete map;
        map = NULL;
    }

    // we have to copy image data if input and output are not the same
    if (sourceImage != targetImage)
        targetImage->copyFrom (sourceImage);
}

unsigned short** PreShadowsHighlightsFilter::getSHMap () {

    return map->rows;
}

unsigned short PreShadowsHighlightsFilter::getMapMax () {

    return max;
}

unsigned short PreShadowsHighlightsFilter::getMapMin () {

    return min;
}

unsigned short PreShadowsHighlightsFilter::getMapAvg () {

    return avg;
}

void PreShadowsHighlightsFilter::getReqiredBufferSize (int& w, int& h) {

    if (procParams->sh.enabled) {
        int sw = getSourceImageView().getPixelWidth ();
        int sh = getSourceImageView().getPixelHeight ();
        if (!procParams->sh.hq) {
            if (sh > sw) {
                w = 2;
                h = sh*omp_get_max_threads();
            }
            else {
                w = sw*omp_get_max_threads();
                h = 2;
            }
        }
        else {
            w = sw;
            h = sh;
        }
    }
    else {
        w = 0;
        h = 0;
    }
}

ShadowsHighlightsFilter::ShadowsHighlightsFilter (PreShadowsHighlightsFilter* pshf)
	: Filter (&toneCurveFilterDescriptor), pshFilter (pshf) {
}

void ShadowsHighlightsFilter::process (const std::set<ProcEvent>& events, MultiImage* sourceImage, MultiImage* targetImage, Buffer<int>* buffer) {

    // apply filter
    int h_th, s_th;
    if (shmap) {
        h_th = pshFilter->getMapMax() - procParams->sh.htonalwidth * (pshFilter->getMapMax() - pshFilter->getMapAvg()) / 100;
        s_th = procParams->sh.stonalwidth * (pshFilter->getMapAvg() - pshFilter->getMapMin()) / 100;
    }

    bool processSH  = procParams->sh.enabled && (procParams->sh.highlights>0 || procParams->sh.shadows>0);
    bool processLCE = procParams->sh.enabled && procParams->sh.localcontrast>0;
    double lceamount = procParams->sh.localcontrast / 200.0;

    if (processSH || processLCE) {

        unsigned short** shmap = pshFilter->getSHMap ();
        TMatrix wprof = iccStore.workingSpaceMatrix (procParams->icm.working);
        double lumimulr = wprof[0][1];
        double lumimulg = wprof[1][1];
        double lumimulb = wprof[2][1];

        #pragma omp parallel for if (multiThread)
        for (int i=0; i<sourceImage->height; i++) {
            for (int j=0; j<sourceImage->width; j++) {
                int r = sourceImage->r[i][j];
                int g = sourceImage->g[i][j];
                int b = sourceImage->b[i][j];
                int mapval = shmap[i][j];
                double factor = 1.0;
                if (processSH) {
                    if (mapval > h_th)
                        factor = (h_th + (100.0 - procParams->sh.highlights) * (mapval - h_th) / 100.0) / mapval;
                    else if (mapval < s_th)
                        factor = (s_th - (100.0 - procParams->sh.shadows) * (s_th - mapval) / 100.0) / mapval;
                }
                if (processLCE) {
                    double sub = lceamount*(mapval-factor*(r*lumimulr + g*lumimulg + b*lumimulb));
                    r = CLIP((int)(factor*r-sub));
                    g = CLIP((int)(factor*g-sub));
                    b = CLIP((int)(factor*b-sub));
                }
                else {
                    r = CLIP((int)(factor*r));
                    g = CLIP((int)(factor*g));
                    b = CLIP((int)(factor*b));
                }
                targetImage->r[i][j] = r;
                targetImage->g[i][j] = g;
                targetImage->b[i][j] = b;
            }
        }
    }
    else if (sourceImage != targetImage)
        targetImage->copyFrom (sourceImage);
}

}