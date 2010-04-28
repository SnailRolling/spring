/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */

#include "StdAfx.h"
#include "CollisionVolume.h"
#include "LogOutput.h"
#include "mmgr.h"


static CLogSubsystem LOG_COLVOL("CollisionVolume");


CR_BIND(CollisionVolume, );
CR_REG_METADATA(CollisionVolume, (
	CR_MEMBER(axisScales),
	CR_MEMBER(axisHScales),
	CR_MEMBER(axisHScalesSq),
	CR_MEMBER(axisHIScales),
	CR_MEMBER(axisOffsets),
	CR_MEMBER(volumeBoundingRadius),
	CR_MEMBER(volumeBoundingRadiusSq),
	CR_MEMBER(volumeType),
	CR_MEMBER(testType),
	CR_MEMBER(primaryAxis),
	CR_MEMBER(secondaryAxes),
	CR_MEMBER(disabled)
));

std::pair<int, int> CollisionVolume::GetVolumeTypeForString(const std::string& volumeTypeStr) {
	std::pair<int, int> p(COLVOL_TYPE_FOOTPRINT, COLVOL_AXIS_Z);

	if (volumeTypeStr.size() > 0) {
		std::string lcVolumeTypeStr(StringToLower(volumeTypeStr));

		if (lcVolumeTypeStr.find("ell") != std::string::npos) {
			p.first = COLVOL_TYPE_ELLIPSOID;
		}

		if (lcVolumeTypeStr.find("cyl") != std::string::npos) {
			p.first = COLVOL_TYPE_CYLINDER;

			if (lcVolumeTypeStr.size() == 4) {
				if (lcVolumeTypeStr[3] == 'x') { p.second = COLVOL_AXIS_X; }
				if (lcVolumeTypeStr[3] == 'y') { p.second = COLVOL_AXIS_Y; }
				if (lcVolumeTypeStr[3] == 'z') { p.second = COLVOL_AXIS_Z; }
			}
		}

		if (lcVolumeTypeStr.find("box") != std::string::npos) {
			p.first = COLVOL_TYPE_BOX;
		}

		if (lcVolumeTypeStr.find("footprint") != std::string::npos) {
			p.first = COLVOL_TYPE_FOOTPRINT;
		}
	}

	return p;
}

// base ctor (CREG-only)
CollisionVolume::CollisionVolume()
{
	axisScales             = float3(2.0f, 2.0f, 2.0f);
	axisHScales            = float3(1.0f, 1.0f, 1.0f);
	axisHScalesSq          = float3(1.0f, 1.0f, 1.0f);
	axisHIScales           = float3(1.0f, 1.0f, 1.0f);
	axisOffsets            = ZeroVector;
	volumeBoundingRadius   = 1.0f;
	volumeBoundingRadiusSq = 1.0f;
	volumeType             = COLVOL_TYPE_ELLIPSOID;
	testType               = COLVOL_TEST_DISC;
	primaryAxis            = COLVOL_AXIS_Z;
	secondaryAxes[0]       = COLVOL_AXIS_X;
	secondaryAxes[1]       = COLVOL_AXIS_Y;
	disabled               = false;
}

// copy ctor
CollisionVolume::CollisionVolume(const CollisionVolume* v, float defRadius)
{
	axisScales             = v->axisScales;
	axisHScales            = v->axisHScales;
	axisHScalesSq          = v->axisHScalesSq;
	axisHIScales           = v->axisHIScales;
	axisOffsets            = v->axisOffsets;
	volumeBoundingRadius   = v->volumeBoundingRadius;
	volumeBoundingRadiusSq = v->volumeBoundingRadiusSq;
	volumeType             = v->volumeType;
	testType               = v->testType;
	primaryAxis            = v->primaryAxis;
	secondaryAxes[0]       = v->secondaryAxes[0];
	secondaryAxes[1]       = v->secondaryAxes[1];
	disabled               = v->disabled;

	// if the volume being copied was not given
	// explicit scales, convert the clone into a
	// sphere if provided with a non-zero radius
	if (axisScales == float3(1.0f, 1.0f, 1.0f) && defRadius > 0.0f) {
		SetDefaultScale(defRadius);
	}
}

CollisionVolume::CollisionVolume(const std::string& typeStr, const float3& scales, const float3& offsets, int testType)
{
	std::pair<int, int> p = CollisionVolume::GetVolumeTypeForString(typeStr);

	switch (p.first) {
		case COLVOL_TYPE_ELLIPSOID: { logOutput.Print(LOG_COLVOL, "New ellipsoid"); } break;
		case COLVOL_TYPE_CYLINDER:  { logOutput.Print(LOG_COLVOL, "New cylinder");  } break;
		case COLVOL_TYPE_BOX:       { logOutput.Print(LOG_COLVOL, "New box");       } break;
		case COLVOL_TYPE_FOOTPRINT: { logOutput.Print(LOG_COLVOL, "New footprint"); } break;
		default: { } break;
	}

	Init(scales, offsets, p.first, testType, p.second);
}


void CollisionVolume::SetDefaultScale(const float s)
{
	// called iif unit or feature defines no custom volume,
	// <s> is the object's default RADIUS (not its diameter)
	// so we need to double it to get the full-length scales
	const float3 scales(s * 2.0f, s * 2.0f, s * 2.0f);

	Init(scales, ZeroVector, volumeType, testType, primaryAxis);
}


void CollisionVolume::Init(const float3& scales, const float3& offsets, int vType, int tType, int pAxis)
{
	//logOutput.Print(LOG_COLVOL, "Init(scales={%g,%g,%g}, offsets={%g,%g,%g}, vType=%d, tType=%d, pAxis=%d)",
	//                scales.x, scales.y, scales.z, offsets.x, offsets.y, offsets.z, vType, tType, pAxis);

	// assign these here, since we can be
	// called from outside the constructor
	primaryAxis = std::max(pAxis, 0) % COLVOL_NUM_AXES;
	volumeType  = std::max(vType, 0) % COLVOL_NUM_TYPES;
	testType    = std::max(tType, 0) % COLVOL_NUM_TESTS;

	// allow defining a custom volume without using it for coldet
	disabled    = (scales.x < 0.0f || scales.y < 0.0f || scales.z < 0.0f);
	axisOffsets = offsets;

	// make sure none of the scales are ever negative
	// or zero; if the resulting vector is <1, 1, 1>
	// then the unit / feature loaders will override
	// the (clone) scales with the model's radius
	SetAxisScales(std::max(1.0f, scales.x), std::max(1.0f, scales.y), std::max(1.0f, scales.z));

	if (volumeType == COLVOL_TYPE_ELLIPSOID) {
		// if all axes (or half-axes) are equal in scale,
		// volume is a sphere (a special-case ellipsoid)
		if ((streflop::fabsf(axisHScales.x - axisHScales.y) < EPS) &&
		    (streflop::fabsf(axisHScales.y - axisHScales.z) < EPS)) {

			logOutput.Print(LOG_COLVOL, "auto-converting spherical COLVOL_TYPE_ELLIPSOID to COLVOL_TYPE_SPHERE");
			volumeType = COLVOL_TYPE_SPHERE;
		}
	}


	// secondaryAxes[0] = (primaryAxis + 1) % 3;
	// secondaryAxes[1] = (primaryAxis + 2) % 3;

	switch (primaryAxis) {
		case COLVOL_AXIS_X: {
			secondaryAxes[0] = COLVOL_AXIS_Y;
			secondaryAxes[1] = COLVOL_AXIS_Z;
		} break;
		case COLVOL_AXIS_Y: {
			secondaryAxes[0] = COLVOL_AXIS_X;
			secondaryAxes[1] = COLVOL_AXIS_Z;
		} break;
		case COLVOL_AXIS_Z: {
			secondaryAxes[0] = COLVOL_AXIS_X;
			secondaryAxes[1] = COLVOL_AXIS_Y;
		} break;
	}

	SetBoundingRadius();
}

void CollisionVolume::SetBoundingRadius() {
	// set the radius of the minimum bounding sphere
	// that encompasses this custom collision volume
	// (for early-out testing)
	switch (volumeType) {
		case COLVOL_TYPE_BOX: {
			// would be an over-estimation for cylinders
			volumeBoundingRadiusSq = axisHScalesSq.x + axisHScalesSq.y + axisHScalesSq.z;
			volumeBoundingRadius = math::sqrt(volumeBoundingRadiusSq);
		} break;
		case COLVOL_TYPE_CYLINDER: {
			const float prhs = axisHScales[primaryAxis     ];   // primary axis half-scale
			const float sahs = axisHScales[secondaryAxes[0]];   // 1st secondary axis half-scale
			const float sbhs = axisHScales[secondaryAxes[1]];   // 2nd secondary axis half-scale
			const float mshs = std::max(sahs, sbhs);            // max. secondary axis half-scale

			volumeBoundingRadiusSq = prhs * prhs + mshs * mshs;
			volumeBoundingRadius = math::sqrt(volumeBoundingRadiusSq);
		} break;
		case COLVOL_TYPE_ELLIPSOID: {
			volumeBoundingRadius = std::max(axisHScales.x, std::max(axisHScales.y, axisHScales.z));
			volumeBoundingRadiusSq = volumeBoundingRadius * volumeBoundingRadius;
		} break;
		case COLVOL_TYPE_FOOTPRINT:
			// fall through, this is intersection of footprint-prism
			// and sphere, so it has same bounding radius as sphere.
		case COLVOL_TYPE_SPHERE: {
			// max{x, y, z} would suffice here too (see ellipsoid)
			volumeBoundingRadius = axisHScales.x;
			volumeBoundingRadiusSq = volumeBoundingRadius * volumeBoundingRadius;
		} break;
	}
}

void CollisionVolume::SetAxisScales(float xs, float ys, float zs) {
	axisScales.x = xs;
	axisScales.y = ys;
	axisScales.z = zs;

	axisHScales.x = axisScales.x * 0.5f;
	axisHScales.y = axisScales.y * 0.5f;
	axisHScales.z = axisScales.z * 0.5f;

	axisHScalesSq.x = axisHScales.x * axisHScales.x;
	axisHScalesSq.y = axisHScales.y * axisHScales.y;
	axisHScalesSq.z = axisHScales.z * axisHScales.z;

	axisHIScales.x = 1.0f / axisHScales.x;
	axisHIScales.y = 1.0f / axisHScales.y;
	axisHIScales.z = 1.0f / axisHScales.z;
}

void CollisionVolume::RescaleAxes(float xs, float ys, float zs) {
	axisScales.x *= xs; axisHScales.x *= xs;
	axisScales.y *= ys; axisHScales.y *= ys;
	axisScales.z *= zs; axisHScales.z *= zs;

	axisHScalesSq.x *= (xs * xs);
	axisHScalesSq.y *= (ys * ys);
	axisHScalesSq.z *= (zs * zs);

	axisHIScales.x *= (1.0f / xs);
	axisHIScales.y *= (1.0f / ys);
	axisHIScales.z *= (1.0f / zs);

	SetBoundingRadius();
}
