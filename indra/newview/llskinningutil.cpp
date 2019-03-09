/** 
* @file llskinningutil.cpp
* @brief  Functions for mesh object skinning
* @author vir@lindenlab.com
*
* $LicenseInfo:firstyear=2015&license=viewerlgpl$
* Second Life Viewer Source Code
* Copyright (C) 2015, Linden Research, Inc.
*
* This library is free software; you can redistribute it and/or
* modify it under the terms of the GNU Lesser General Public
* License as published by the Free Software Foundation;
* version 2.1 of the License only.
*
* This library is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
* Lesser General Public License for more details.
*
* You should have received a copy of the GNU Lesser General Public
* License along with this library; if not, write to the Free Software
* Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*
* Linden Research, Inc., 945 Battery Street, San Francisco, CA  94111  USA
* $/LicenseInfo$
*/

#include "llviewerprecompiledheaders.h"

#include "llskinningutil.h"
#include "llvoavatar.h"
#include "llviewercontrol.h"
#include "llmeshrepository.h"

// static
void LLSkinningUtil::initClass()
{
}

U32 LLSkinningUtil::getMaxJointCount()
{
    U32 result = LL_MAX_JOINTS_PER_MESH_OBJECT;
	return result;
}

U32 LLSkinningUtil::getMeshJointCount(const LLMeshSkinInfo *skin)
{
	return llmin((U32)getMaxJointCount(), (U32)skin->mJointNames.size());
}

void LLSkinningUtil::scrubInvalidJoints(LLVOAvatar *avatar, LLMeshSkinInfo* skin)
{
    if (skin->mInvalidJointsScrubbed)
    {
        return;
    }
    for (U32 j = 0; j < skin->mJointNames.size(); ++j)
    {
        // Fix invalid names to "mPelvis". Currently meshes with
        // invalid names will be blocked on upload, so this is just
        // needed for handling of any legacy bad data.
        if (!avatar->getJoint(skin->mJointNames[j]))
        {
            LL_DEBUGS("Avatar") << avatar->getFullname() << " mesh rigged to invalid joint " << skin->mJointNames[j] << LL_ENDL;
            LL_WARNS_ONCE("Avatar") << avatar->getFullname() << " mesh rigged to invalid joint" << skin->mJointNames[j] << LL_ENDL;
            skin->mJointNames[j] = "mPelvis";
            skin->mJointNumsInitialized = false; // force update after names change.
        }
    }
    skin->mInvalidJointsScrubbed = true;
}

// static
void LLSkinningUtil::initSkinningMatrixPalette(
    LLMatrix4a* mat,
    S32 count, 
    const LLMeshSkinInfo* skin,
    LLVOAvatar *avatar,
    bool relative_to_avatar)
{
    initJointNums(const_cast<LLMeshSkinInfo*>(skin), avatar);
    for (U32 j = 0; j < (U32)count; ++j)
    {
        LLJoint *joint = avatar->getJoint(skin->mJointNums[j]);
        if (joint)
        {
            LLMatrix4a bind;
            bind.loadu((F32*)skin->mInvBindMatrix[j].mMatrix);
            if (relative_to_avatar)
            {
            	LLMatrix4a trans = joint->getWorldMatrix();
            	trans.translate_affine(avatar->getPosition() * -1.f);
            	mat[j].setMul(trans, bind);
            }
            else
                mat[j].setMul(joint->getWorldMatrix(), bind);
        }
        else
        {
            mat[j].loadu((F32*)skin->mInvBindMatrix[j].mMatrix);
            // This  shouldn't  happen   -  in  mesh  upload,  skinned
            // rendering  should  be disabled  unless  all joints  are
            // valid.  In other  cases of  skinned  rendering, invalid
            // joints should already have  been removed during scrubInvalidJoints().
            LL_WARNS_ONCE("Avatar") << avatar->getFullname() 
                                    << " rigged to invalid joint name " << skin->mJointNames[j] 
                                    << " num " << skin->mJointNums[j] << LL_ENDL;
            LL_WARNS_ONCE("Avatar") << avatar->getFullname() 
                                    << " avatar build state: isBuilt() " << avatar->isBuilt() 
                                    << " mInitFlags " << avatar->mInitFlags << LL_ENDL;
        }
    }
}


void LLSkinningUtil::checkSkinWeights(const LLVector4a* weights, U32 num_vertices, const LLMeshSkinInfo* skin)
{
#ifndef LL_RELEASE_FOR_DOWNLOAD
	const S32 max_joints = skin->mJointNames.size();
    for (U32 j=0; j<num_vertices; j++)
    {
        const F32 *w = weights[j].getF32ptr();
            
        F32 wsum = 0.0;
        for (U32 k=0; k<4; ++k)
        {
            S32 i = llfloor(w[k]);
            llassert(i>=0);
            llassert(i<max_joints);
            wsum += w[k]-i;
        }
        llassert(wsum > 0.0f);
    }
#endif
}

void LLSkinningUtil::scrubSkinWeights(LLVector4a* weights, U32 num_vertices, const LLMeshSkinInfo* skin)
{
    const S32 max_joints = skin->mJointNames.size();
    for (U32 j=0; j<num_vertices; j++)
    {
        F32 *w = weights[j].getF32ptr();

        for (U32 k=0; k<4; ++k)
        {
            S32 i = llfloor(w[k]);
            F32 f = w[k]-i;
            i = llclamp(i,0,max_joints-1);
            w[k] = i + f;
        }
    }
	checkSkinWeights(weights, num_vertices, skin);
}

void LLSkinningUtil::getPerVertexSkinMatrix(
    const F32* weights,
    LLMatrix4a* mat,
    bool handle_bad_scale,
    LLMatrix4a& final_mat,
    U32 max_joints)
{
    bool valid_weights = true;
	final_mat.clear();

	S32 idx[4];

	LLVector4 wght;

	F32 scale = 0.f;
	for (U32 k = 0; k < 4; k++)
	{
		F32 w = weights[k];

		idx[k] = (S32) w;

		wght[k] = w - idx[k];
		scale += wght[k];
	}
	if (handle_bad_scale && scale <= 0.f)
	{
		wght = LLVector4(F32_MAX,F32_MAX,F32_MAX,F32_MAX);
		valid_weights = false;
	}
	else
	{
		// This is enforced  in unpackVolumeFaces()
		llassert(scale>0.f);

		wght *= 1.f/scale;
	}

	for (U32 k = 0; k < 4; k++)
	{
		F32 w = wght[k];

		LLMatrix4a src;
		src.setMul(mat[idx[k]], w);

		final_mat.add(src);
	}
	// SL-366 - with weight validation/cleanup code, it should no longer be
	// possible to hit the bad scale case.
	llassert(valid_weights);
}

void LLSkinningUtil::initJointNums(LLMeshSkinInfo* skin, LLVOAvatar *avatar)
{
    if (!skin->mJointNumsInitialized)
    {
        for (U32 j = 0; j < skin->mJointNames.size(); ++j)
        {
            LLJoint *joint = NULL;
            if (skin->mJointNums[j] == -1)
            {
                joint = avatar->getJoint(skin->mJointNames[j]);
                if (joint)
                {
                    skin->mJointNums[j] = joint->getJointNum();
                    if (skin->mJointNums[j] < 0)
                    {
                        LL_WARNS_ONCE("Avatar") << avatar->getFullname() << " joint has unusual number " << skin->mJointNames[j] << ": " << skin->mJointNums[j] << LL_ENDL;
                        LL_WARNS_ONCE("Avatar") << avatar->getFullname() << " avatar build state: isBuilt() " << avatar->isBuilt() << " mInitFlags " << avatar->mInitFlags << LL_ENDL;
                    }
                }
                else
                {
                    LL_WARNS_ONCE("Avatar") << avatar->getFullname() << " unable to find joint " << skin->mJointNames[j] << LL_ENDL;
                    LL_WARNS_ONCE("Avatar") << avatar->getFullname() << " avatar build state: isBuilt() " << avatar->isBuilt() << " mInitFlags " << avatar->mInitFlags << LL_ENDL;
#if 0
                    dump_avatar_and_skin_state("initJointNums joint not found", avatar, skin);
#endif
                }
            }
        }
        skin->mJointNumsInitialized = true;
    }
}

// This is used for extracting rotation from a bind shape matrix that
// already has scales baked in
LLQuaternion LLSkinningUtil::getUnscaledQuaternion(const LLMatrix4& mat4)
{
    LLMatrix3 bind_mat = mat4.getMat3();
    for (auto i = 0; i < 3; i++)
    {
        F32 len = 0.0f;
        for (auto j = 0; j < 3; j++)
        {
            len += bind_mat.mMatrix[i][j] * bind_mat.mMatrix[i][j];
        }
        if (len > 0.0f)
        {
            len = sqrt(len);
            for (auto j = 0; j < 3; j++)
            {
                bind_mat.mMatrix[i][j] /= len;
            }
        }
    }
    bind_mat.invert();
    LLQuaternion bind_rot = bind_mat.quaternion();
    bind_rot.normalize();
    return bind_rot;
}
