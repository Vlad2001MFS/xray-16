#include "stdafx.h"
#pragma hdrstop

#include "xrCore/buffer_vector.h"

#include "ResourceManager.h"
#ifdef _EDITOR
#include "Include/xrAPI/xrAPI.h"
#endif

void CResourceManager::reset_begin()
{
    // destroy state-blocks
    for (SState* sstate : v_states)
        _RELEASE(sstate->state);

    // destroy RTs
    for (auto& rt_pair : m_rtargets)
        rt_pair.second->reset_begin();
    //  DX10 cut    for (map_RTCIt rtc_it=m_rtargets_c.begin(); rtc_it!=m_rtargets_c.end(); rtc_it++)
    //  DX10 cut        rtc_it->second->reset_begin();

    // destroy DStreams
    RImplementation.old_QuadIB = RImplementation.QuadIB;
    RImplementation.QuadIB.Release();

    RImplementation.Index.reset_begin();
    RImplementation.Vertex.reset_begin();
}

bool cmp_rt(const CRT* A, const CRT* B) { return A->_order < B->_order; }
//  DX10 cut bool   cmp_rtc (const CRTC* A,const CRTC* B)   { return A->_order < B->_order; }

void CResourceManager::reset_end()
{
    // create RDStreams
    RImplementation.Vertex.reset_end();
    RImplementation.Index.reset_end();
    Evict();
    RImplementation.CreateQuadIB();

    // remark geom's which point to dynamic VB/IB
    {
        for (u32 _it = 0; _it < v_geoms.size(); _it++)
        {
            SGeometry* _G = v_geoms[_it];
            if (_G->vb == RImplementation.Vertex.old_pVB)
                _G->vb = RImplementation.Vertex.Buffer();

            // Here we may recover the buffer using one of
            // RCache's index buffers.
            // Do not remove else.
            if (_G->ib == RImplementation.Index.old_pIB)
            {
                _G->ib = RImplementation.Index.Buffer();
            }
            else if (_G->ib == RImplementation.old_QuadIB)
            {
                _G->ib = RImplementation.QuadIB;
            }
        }
    }

    // create RTs in the same order as them was first created
    {
        // RT
        const size_t buffer_size = m_rtargets.size() * sizeof(CRT*);
        buffer_vector<CRT*> sorted_rts(xr_alloca(buffer_size), m_rtargets.size());
        for (auto& rt_pair : m_rtargets)
            sorted_rts.push_back(rt_pair.second);

        std::sort(sorted_rts.begin(), sorted_rts.end(), cmp_rt);

        for (CRT* rt : sorted_rts)
            rt->reset_end();
    }
    {
        // RTc
    //DX10 cut		const size_t buffer_size = m_rtargets_c.size() * sizeof(CRTC*);
    //DX10 cut		buffer_vector<CRTC*> sorted_rtcs(xr_alloca(buffer_size), m_rtargets_c.size());
    //DX10 cut		for (map_RTCIt rt_it=m_rtargets_c.begin(); rt_it!=m_rtargets_c.end(); rt_it++)	sorted_rtcs.push_back(rt_it->second);
    //DX10 cut		std::sort(sorted_rtcs.begin(),sorted_rtcs.end(),cmp_rtc);
    //DX10 cut		for (u32 _it=0; _it<sorted_rtcs.size(); _it++)	sorted_rtcs[_it]->reset_end	();
    }

    // create state-blocks
    for (SState* sstate : v_states)
    {
        sstate->state_code.record(sstate->state);
    }

    // create everything, renderer may use
    GEnv.Render->reset_end();
#ifndef MASTER_GOLD
    Dump(true);
#endif
}

template <class C>
void mdump(C c)
{
    if (0 == c.size())
        return;
    for (auto I = c.begin(); I != c.end(); ++I)
        Msg("*        : %3d: %s", I->second->ref_count.load() , I->second->cName.c_str());
}

CResourceManager::~CResourceManager()
{
    DestroyNecessaryTextures();
#ifndef MASTER_GOLD
    Dump(false);
#endif
}

void CResourceManager::Dump(bool bBrief)
{
    Msg("* RM_Dump: textures  : %d", m_textures.size());
    if (!bBrief)
        mdump(m_textures);
    Msg("* RM_Dump: rtargets  : %d", m_rtargets.size());
    if (!bBrief)
        mdump(m_rtargets);
    //  DX10 cut    Msg("* RM_Dump: rtargetsc : %d", m_rtargets_c.size()); if(!bBrief) mdump(m_rtargets_c);
    Msg("* RM_Dump: vs        : %d", m_vs.size());
    if (!bBrief)
        mdump(m_vs);
    Msg("* RM_Dump: ps        : %d", m_ps.size());
    if (!bBrief)
        mdump(m_ps);
#if defined(USE_DX11) || defined(USE_OGL)
    Msg("* RM_Dump: gs        : %d", m_gs.size());
    if (!bBrief)
        mdump(m_gs);
#    ifdef USE_DX11
    Msg("* RM_Dump: cs        : %d", m_cs.size());
    if (!bBrief)
        mdump(m_cs);
    Msg("* RM_Dump: hs        : %d", m_hs.size());
    if (!bBrief)
        mdump(m_hs);
    Msg("* RM_Dump: ds        : %d", m_ds.size());
    if (!bBrief)
        mdump(m_ds);
#    endif
#endif
    Msg("* RM_Dump: dcl       : %d", v_declarations.size());
    Msg("* RM_Dump: states    : %d", v_states.size());
    Msg("* RM_Dump: tex_list  : %d", lst_textures.size());
    Msg("* RM_Dump: matrices  : %d", lst_matrices.size());
    Msg("* RM_Dump: lst_constants: %d", lst_constants.size());
    Msg("* RM_Dump: v_passes  : %d", v_passes.size());
    Msg("* RM_Dump: v_elements: %d", v_elements.size());
    Msg("* RM_Dump: v_shaders : %d", v_shaders.size());
}
