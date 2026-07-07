// BleachBypass.ofx
//
// Digital emulation of the bleach bypass (silver retention / skip
// bleach) film processing technique, as a compiled OFX plugin so it
// can be dragged directly into DaVinci Resolve's Color page node graph
// like any other OFX effect.
//
// Same recipe as the Fusion Fuse version of this tool: desaturate a
// copy of the image toward luma, push extra contrast into it (standing
// in for the density retained silver halide adds on real film), nudge
// it slightly toward cyan (a characteristic color shift of the real
// process), then composite that layer back over the original with an
// Overlay-family blend mode.

#include <cmath>
#include <cstring>
#include <memory>
#include "ofxsImageEffect.h"
#include "ofxsMultiThread.h"
#include "ofxsProcessing.h"

using namespace OFX;

////////////////////////////////////////////////////////////////////////////
// blend mode math

template <class T>
inline T Clamp01(T v)
{
	return v < T(0) ? T(0) : (v > T(1) ? T(1) : v);
}

static inline float BlendOverlay(float base, float blend)
{
	if (base < 0.5f) {
		return 2.0f * base * blend;
	}
	return 1.0f - 2.0f * (1.0f - base) * (1.0f - blend);
}

static inline float BlendHardLight(float base, float blend)
{
	// Hard Light is Overlay with the base/blend roles swapped.
	return BlendOverlay(blend, base);
}

static inline float BlendSoftLight(float base, float blend)
{
	// Common simplified soft light approximation.
	return (1.0f - 2.0f * blend) * base * base + 2.0f * blend * base;
}

////////////////////////////////////////////////////////////////////////////
class BleachBypassProcessorBase : public OFX::ImageProcessor {
protected:
	OFX::Image *_srcImg;
	double _amount, _desaturation, _density, _tint;
	int _blendMode; // 0=Overlay 1=SoftLight 2=HardLight

public:
	BleachBypassProcessorBase(OFX::ImageEffect &instance)
		: OFX::ImageProcessor(instance)
		, _srcImg(0)
		, _amount(0.75)
		, _desaturation(0.6)
		, _density(0.3)
		, _tint(0.15)
		, _blendMode(0)
	{
	}

	void setSrcImg(OFX::Image *v) { _srcImg = v; }

	void setParams(double amount, double desaturation, double density, double tint, int blendMode)
	{
		_amount = amount;
		_desaturation = desaturation;
		_density = density;
		_tint = tint;
		_blendMode = blendMode;
	}
};

template <class PIX, int nComponents, int maxValue>
class BleachBypassProcessor : public BleachBypassProcessorBase {
public:
	BleachBypassProcessor(OFX::ImageEffect &instance) : BleachBypassProcessorBase(instance) {}

	void multiThreadProcessImages(OfxRectI procWindow)
	{
		const float contrastFactor = 1.0f + (float)_density * 1.5f;
		const float tint = (float)_tint;
		const float amount = (float)_amount;
		const float desat = (float)_desaturation;

		for (int y = procWindow.y1; y < procWindow.y2; y++) {
			if (_effect.abort()) break;

			PIX *dstPix = (PIX *)_dstImg->getPixelAddress(procWindow.x1, y);

			for (int x = procWindow.x1; x < procWindow.x2; x++) {
				PIX *srcPix = (PIX *)(_srcImg ? _srcImg->getPixelAddress(x, y) : 0);

				if (!srcPix) {
					for (int c = 0; c < nComponents; c++) {
						dstPix[c] = 0;
					}
					dstPix += nComponents;
					continue;
				}

				float r = nComponents > 0 ? float(srcPix[0]) / float(maxValue) : 0.0f;
				float g = nComponents > 1 ? float(srcPix[1]) / float(maxValue) : r;
				float b = nComponents > 2 ? float(srcPix[2]) / float(maxValue) : r;
				float a = nComponents > 3 ? float(srcPix[3]) / float(maxValue) : 1.0f;

				// Desaturate toward luma (Rec.709 weights).
				float luma = r * 0.2126f + g * 0.7152f + b * 0.0722f;
				float mr = r + (luma - r) * desat;
				float mg = g + (luma - g) * desat;
				float mb = b + (luma - b) * desat;

				// Silver density: extra contrast pivoted around mid-gray,
				// standing in for the density retained silver would add.
				mr = (mr - 0.5f) * contrastFactor + 0.5f;
				mg = (mg - 0.5f) * contrastFactor + 0.5f;
				mb = (mb - 0.5f) * contrastFactor + 0.5f;

				mr = Clamp01(mr);
				mg = Clamp01(mg);
				mb = Clamp01(mb);

				// Cyan shift: nudge red down / blue up. Using gamma
				// rather than a flat offset leans the shift toward
				// shadows without needing a separate luma mask.
				if (tint > 0.0f) {
					mr = powf(mr, 1.0f + tint * 0.3f);
					mb = powf(mb, 1.0f - tint * 0.2f);
				}

				// Composite the silver layer back over the original.
				float br, bg, bb;
				switch (_blendMode) {
					case 1:
						br = BlendSoftLight(r, mr);
						bg = BlendSoftLight(g, mg);
						bb = BlendSoftLight(b, mb);
						break;
					case 2:
						br = BlendHardLight(r, mr);
						bg = BlendHardLight(g, mg);
						bb = BlendHardLight(b, mb);
						break;
					default:
						br = BlendOverlay(r, mr);
						bg = BlendOverlay(g, mg);
						bb = BlendOverlay(b, mb);
						break;
				}

				// Amount: linear mix between untouched original and the
				// full effect.
				float outR = Clamp01(r + (br - r) * amount);
				float outG = Clamp01(g + (bg - g) * amount);
				float outB = Clamp01(b + (bb - b) * amount);

				if (nComponents > 0) {
					dstPix[0] = maxValue == 1 ? PIX(outR) : PIX(outR * maxValue + 0.5f);
				}
				if (nComponents > 1) {
					dstPix[1] = maxValue == 1 ? PIX(outG) : PIX(outG * maxValue + 0.5f);
				}
				if (nComponents > 2) {
					dstPix[2] = maxValue == 1 ? PIX(outB) : PIX(outB * maxValue + 0.5f);
				}
				if (nComponents > 3) {
					dstPix[3] = maxValue == 1 ? PIX(a) : PIX(a * maxValue + 0.5f);
				}

				dstPix += nComponents;
			}
		}
	}
};

////////////////////////////////////////////////////////////////////////////
class BleachBypassPlugin : public OFX::ImageEffect {
protected:
	OFX::Clip *dstClip_;
	OFX::Clip *srcClip_;

	OFX::DoubleParam *amount_;
	OFX::DoubleParam *desaturation_;
	OFX::DoubleParam *density_;
	OFX::DoubleParam *tint_;
	OFX::ChoiceParam *blendMode_;

public:
	BleachBypassPlugin(OfxImageEffectHandle handle)
		: ImageEffect(handle)
		, dstClip_(0)
		, srcClip_(0)
	{
		dstClip_ = fetchClip(kOfxImageEffectOutputClipName);
		srcClip_ = fetchClip(kOfxImageEffectSimpleSourceClipName);

		amount_ = fetchDoubleParam("amount");
		desaturation_ = fetchDoubleParam("desaturation");
		density_ = fetchDoubleParam("density");
		tint_ = fetchDoubleParam("tint");
		blendMode_ = fetchChoiceParam("blendMode");
	}

	virtual void render(const OFX::RenderArguments &args);
	virtual bool isIdentity(const OFX::IsIdentityArguments &args, OFX::Clip * &identityClip, double &identityTime);

	void setupAndProcess(BleachBypassProcessorBase &, const OFX::RenderArguments &args);
};

void BleachBypassPlugin::setupAndProcess(BleachBypassProcessorBase &processor, const OFX::RenderArguments &args)
{
	std::unique_ptr<OFX::Image> dst(dstClip_->fetchImage(args.time));
	OFX::BitDepthEnum dstBitDepth = dst->getPixelDepth();
	OFX::PixelComponentEnum dstComponents = dst->getPixelComponents();

	std::unique_ptr<OFX::Image> src(srcClip_->fetchImage(args.time));

	if (src.get()) {
		OFX::BitDepthEnum srcBitDepth = src->getPixelDepth();
		OFX::PixelComponentEnum srcComponents = src->getPixelComponents();
		if (srcBitDepth != dstBitDepth || srcComponents != dstComponents) {
			OFX::throwSuiteStatusException(kOfxStatErrImageFormat);
		}
	}

	double amount = amount_->getValueAtTime(args.time);
	double desaturation = desaturation_->getValueAtTime(args.time);
	double density = density_->getValueAtTime(args.time);
	double tint = tint_->getValueAtTime(args.time);
	int blendMode = 0;
	blendMode_->getValueAtTime(args.time, blendMode);

	processor.setDstImg(dst.get());
	processor.setSrcImg(src.get());
	processor.setRenderWindow(args.renderWindow);
	processor.setParams(amount, desaturation, density, tint, blendMode);

	processor.process();
}

bool BleachBypassPlugin::isIdentity(const OFX::IsIdentityArguments &args, OFX::Clip * &identityClip, double &identityTime)
{
	double amount = amount_->getValueAtTime(args.time);
	if (amount <= 0.0) {
		identityClip = srcClip_;
		identityTime = args.time;
		return true;
	}
	return false;
}

void BleachBypassPlugin::render(const OFX::RenderArguments &args)
{
	OFX::BitDepthEnum dstBitDepth = dstClip_->getPixelDepth();
	OFX::PixelComponentEnum dstComponents = dstClip_->getPixelComponents();

	if (dstComponents == OFX::ePixelComponentRGBA) {
		switch (dstBitDepth) {
			case OFX::eBitDepthUByte: {
				BleachBypassProcessor<unsigned char, 4, 255> p(*this);
				setupAndProcess(p, args);
				break;
			}
			case OFX::eBitDepthUShort: {
				BleachBypassProcessor<unsigned short, 4, 65535> p(*this);
				setupAndProcess(p, args);
				break;
			}
			case OFX::eBitDepthFloat: {
				BleachBypassProcessor<float, 4, 1> p(*this);
				setupAndProcess(p, args);
				break;
			}
			default:
				OFX::throwSuiteStatusException(kOfxStatErrUnsupported);
		}
	} else {
		switch (dstBitDepth) {
			case OFX::eBitDepthUByte: {
				BleachBypassProcessor<unsigned char, 3, 255> p(*this);
				setupAndProcess(p, args);
				break;
			}
			case OFX::eBitDepthUShort: {
				BleachBypassProcessor<unsigned short, 3, 65535> p(*this);
				setupAndProcess(p, args);
				break;
			}
			case OFX::eBitDepthFloat: {
				BleachBypassProcessor<float, 3, 1> p(*this);
				setupAndProcess(p, args);
				break;
			}
			default:
				OFX::throwSuiteStatusException(kOfxStatErrUnsupported);
		}
	}
}

////////////////////////////////////////////////////////////////////////////
mDeclarePluginFactory(BleachBypassPluginFactory, {}, {});

void BleachBypassPluginFactory::describe(OFX::ImageEffectDescriptor &desc)
{
	desc.setLabels("Bleach Bypass", "Bleach Bypass", "Bleach Bypass");
	desc.setPluginGrouping("Lund Films");
	desc.setPluginDescription(
		"Digital emulation of the bleach bypass (silver retention) film "
		"processing technique: desaturated, high-contrast, gritty look "
		"with a slight cyan color shift.");

	desc.addSupportedContext(eContextFilter);
	desc.addSupportedContext(eContextGeneral);

	desc.addSupportedBitDepth(eBitDepthUByte);
	desc.addSupportedBitDepth(eBitDepthUShort);
	desc.addSupportedBitDepth(eBitDepthFloat);

	desc.setSingleInstance(false);
	desc.setHostFrameThreading(false);
	desc.setSupportsMultiResolution(true);
	desc.setSupportsTiles(true);
	desc.setTemporalClipAccess(false);
	desc.setRenderTwiceAlways(false);
	desc.setSupportsMultipleClipPARs(false);
}

static DoubleParamDescriptor *defineUnitParam(
	OFX::ImageEffectDescriptor &desc,
	const std::string &name, const std::string &label, const std::string &hint,
	double defaultValue)
{
	DoubleParamDescriptor *param = desc.defineDoubleParam(name);
	param->setLabels(label, label, label);
	param->setScriptName(name);
	param->setHint(hint);
	param->setDefault(defaultValue);
	param->setRange(0.0, 1.0);
	param->setDisplayRange(0.0, 1.0);
	param->setIncrement(0.01);
	return param;
}

void BleachBypassPluginFactory::describeInContext(OFX::ImageEffectDescriptor &desc, OFX::ContextEnum context)
{
	ClipDescriptor *srcClip = desc.defineClip(kOfxImageEffectSimpleSourceClipName);
	srcClip->addSupportedComponent(ePixelComponentRGBA);
	srcClip->addSupportedComponent(ePixelComponentRGB);
	srcClip->setTemporalClipAccess(false);
	srcClip->setSupportsTiles(true);
	srcClip->setIsMask(false);

	ClipDescriptor *dstClip = desc.defineClip(kOfxImageEffectOutputClipName);
	dstClip->addSupportedComponent(ePixelComponentRGBA);
	dstClip->addSupportedComponent(ePixelComponentRGB);
	dstClip->setSupportsTiles(true);

	PageParamDescriptor *page = desc.definePageParam("Controls");

	DoubleParamDescriptor *param;

	param = defineUnitParam(desc, "amount", "Amount", "Overall effect strength", 0.75);
	page->addChild(*param);

	param = defineUnitParam(desc, "desaturation", "Desaturation", "How monochrome the blended silver layer is", 0.6);
	page->addChild(*param);

	param = defineUnitParam(desc, "density", "Silver Density",
		"Extra contrast pushed into the silver layer before blending, standing in for the density retained silver adds on real film", 0.3);
	page->addChild(*param);

	param = defineUnitParam(desc, "tint", "Cyan Shift",
		"Subtle color balance shift toward cyan, a characteristic of the real process", 0.15);
	page->addChild(*param);

	ChoiceParamDescriptor *blendChoice = desc.defineChoiceParam("blendMode");
	blendChoice->setLabels("Blend Mode", "Blend Mode", "Blend Mode");
	blendChoice->setHint("Overlay is the standard/classic choice; Soft Light is gentler, Hard Light more aggressive");
	blendChoice->appendOption("Overlay");
	blendChoice->appendOption("Soft Light");
	blendChoice->appendOption("Hard Light");
	blendChoice->setDefault(0);
	page->addChild(*blendChoice);
}

ImageEffect *BleachBypassPluginFactory::createInstance(OfxImageEffectHandle handle, ContextEnum /*context*/)
{
	return new BleachBypassPlugin(handle);
}

namespace OFX {
	namespace Plugin {
		void getPluginIDs(OFX::PluginFactoryArray &ids)
		{
			static BleachBypassPluginFactory p("fi.lundfilms.BleachBypass", 1, 0);
			ids.push_back(&p);
		}
	}
}
