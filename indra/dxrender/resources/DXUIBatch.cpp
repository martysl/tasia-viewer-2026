#include "DXUIBatch.h"
#include "DXDevice.h"
#include "DXSwapChain.h"
#include "DXVertexLayout.h"
#include "DXStateCache.h"
#include "llerror.h"

DXUIBatch gDXUIBatch;

void DXUIBatch::push(const DXUIVertex* vertices, size_t count)
{
    mVertices.insert(mVertices.end(), vertices, vertices + count);
    mLastPushCount = count;
}

namespace
{
    D3D11_PRIMITIVE_TOPOLOGY toD3DTopology(DXUITopology topology)
    {
        switch (topology)
        {
        case DXUITopology::TriangleStrip: return D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;
        case DXUITopology::LineList:      return D3D11_PRIMITIVE_TOPOLOGY_LINELIST;
        case DXUITopology::LineStrip:     return D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP;
        case DXUITopology::TriangleList:
        default:                          return D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
        }
    }
}

void DXUIBatch::flush(const void* vs_bytecode, size_t vs_bytecode_size, ID3D11VertexShader* vs, ID3D11PixelShader* ps, bool alpha_blend, const char* debug_name, DXUITopology topology,
    bool depth_test, bool depth_write, D3D11_COMPARISON_FUNC depth_func, void* texture0, void* sampler0, void* constant_buffer, UINT constant_buffer_slot)
{
    BatchState new_state;
    new_state.vs = vs;
    new_state.ps = ps;
    new_state.vsBytecode = vs_bytecode;
    new_state.vsBytecodeSize = vs_bytecode_size;
    new_state.debugName = debug_name;
    new_state.topology = topology;
    new_state.alphaBlend = alpha_blend;
    new_state.depthTest = depth_test;
    new_state.depthWrite = depth_write;
    new_state.depthFunc = depth_func;
    new_state.texture0 = texture0;
    new_state.sampler0 = sampler0;
    new_state.constantBuffer = constant_buffer;
    new_state.constantBufferSlot = constant_buffer_slot;

    // The vertices for THIS call's primitive are the tail-end mLastPushCount
    // entries (the push() that must have immediately preceded this flush()
    // call, per this class's documented push()+flush() pairing contract).
    // Anything before that belongs to whatever batch was already pending.
    size_t new_count = mLastPushCount;
    size_t old_count = (mVertices.size() >= new_count) ? (mVertices.size() - new_count) : 0;

    if (mHasPending && old_count > 0)
    {
        if (new_state.sameDrawState(mPendingState))
        {
            // Same shader/topology/blend/depth as what's already pending -
            // just keep accumulating, don't draw yet. mVertices already has
            // the new vertices appended (push() already ran) - nothing else
            // to do.
            mPendingState = new_state; // refresh bytecode/debugName pointers
            return;
        }

        // State changed - draw the old batch now, using ITS OWN state, then
        // drop those vertices and continue with the new ones as a fresh
        // pending batch.
        drawAndPop(old_count, mPendingState);
    }

    mPendingState = new_state;
    mHasPending = true;
}

void DXUIBatch::flushPending()
{
    if (!mHasPending || mVertices.empty())
    {
        mHasPending = false;
        mVertices.clear();
        return;
    }

    drawAndPop(mVertices.size(), mPendingState);
    mHasPending = false;
}

void DXUIBatch::drawAndPop(size_t count, const BatchState& state)
{
    if (count == 0 || count > mVertices.size())
    {
        return;
    }

    static bool logged_first_draw = false;
    if (!logged_first_draw)
    {
        logged_first_draw = true;
        LL_INFOS("DXUI") << "Submitting first DX UI batch: " << count
            << " via " << (state.debugName ? state.debugName : "unnamed shader")
            << " vertices"
            << LL_ENDL;
    }

    const size_t byte_size = count * sizeof(DXUIVertex);
    if (count > mVertexCapacity)
    {
        if (!mVertexBuffer.createVertexBuffer(byte_size, nullptr))
        {
            mVertices.erase(mVertices.begin(), mVertices.begin() + count);
            return;
        }
        mVertexCapacity = count;
    }

    if (!mVertexBuffer.upload(mVertices.data(), byte_size))
    {
        mVertices.erase(mVertices.begin(), mVertices.begin() + count);
        return;
    }

    ID3D11DeviceContext* ctx = gDXDevice.getContext();

    // S24 (2026-08-02): set unconditionally, every call - see this
    // function's header comment for why trusting the caller's own
    // shader.bind() call was found to be unverifiable (bind()'s
    // sCurBoundShaderPtr-based early-out can silently no-op if anything
    // else touched VS/PS in between without updating that bookkeeping).
    // This is the single submission chokepoint for all of 2D UI/text, so
    // guaranteeing it here closes the whole bug class at once rather than
    // chasing every place that could theoretically leave stale state.
    ctx->VSSetShader(state.vs, nullptr, 0);
    ctx->PSSetShader(state.ps, nullptr, 0);

    // Re-bind the texture/sampler this batch was built for.  Deferred draws
    // must not inherit whatever slot 0 happens to hold by draw time - see
    // BatchState::texture0's comment for the concrete failure this fixes.
    if (state.texture0)
    {
        ID3D11ShaderResourceView* srv = (ID3D11ShaderResourceView*)state.texture0;
        ctx->PSSetShaderResources(0, 1, &srv);
    }
    if (state.sampler0)
    {
        ID3D11SamplerState* sampler = (ID3D11SamplerState*)state.sampler0;
        ctx->PSSetSamplers(0, 1, &sampler);
    }
    if (state.constantBuffer)
    {
        // The MVP this batch's vertices were built with, not whatever the
        // currently bound shader uploaded last.
        ID3D11Buffer* cbuffer = (ID3D11Buffer*)state.constantBuffer;
        ctx->VSSetConstantBuffers(state.constantBufferSlot, 1, &cbuffer);
    }

    ID3D11InputLayout* layout = DXVertexLayout::getOrCreateUILayout(state.vsBytecode, state.vsBytecodeSize, state.debugName);
    ctx->IASetInputLayout(layout);

    static bool logged_submission_target = false;
    if (!logged_submission_target)
    {
        logged_submission_target = true;
        ID3D11RenderTargetView* target = nullptr;
        ctx->OMGetRenderTargets(1, &target, nullptr);
        LL_INFOS("DXUI") << "First UI submission: input layout=" << (layout ? "valid" : "MISSING")
            << ", target=" << (target == gDXSwapChain.getBackBufferRTV() ? "back buffer" : "NOT back buffer")
            << LL_ENDL;
        if (target)
        {
            target->Release();
        }
    }

    ID3D11Buffer* vb = mVertexBuffer.getBuffer();
    UINT stride = sizeof(DXUIVertex);
    UINT offset = 0;
    ctx->IASetVertexBuffers(0, 1, &vb, &stride, &offset);
    DXStateCache::setPrimitiveTopology(ctx, toD3DTopology(state.topology));

    // UI vertices are emitted in the viewer's Y-up screen convention, while
    // D3D11's viewport maps Y downward. Never inherit culling or a stale
    // scissor rectangle from a preceding 3D/post-process pass: either can
    // discard every otherwise-valid UI primitive before the pixel shader.
    ctx->RSSetState(DXStateCache::getRasterizerState(false, false));

    // Matches GL's BT_ALPHA (LLRender::setSceneBlendType) - src alpha /
    // inverse src alpha, full RGBA write mask. Non-blended (alpha_blend =
    // false) draws (e.g. opaque icon backgrounds) get straight overwrite.
    ID3D11BlendState* bs = state.alphaBlend
        ? DXStateCache::getBlendState(true, D3D11_BLEND_SRC_ALPHA, D3D11_BLEND_INV_SRC_ALPHA, D3D11_BLEND_SRC_ALPHA, D3D11_BLEND_INV_SRC_ALPHA, 0xF)
        : DXStateCache::getBlendState(false, D3D11_BLEND_ONE, D3D11_BLEND_ZERO, D3D11_BLEND_ONE, D3D11_BLEND_ZERO, 0xF);
    ctx->OMSetBlendState(bs, nullptr, 0xFFFFFFFF);

    // S24 (DX_RENDER, 2026-07-30): this is a dedicated 2D screen-space UI
    // batch renderer - it should never be depth-culled by 3D scene content
    // regardless of whatever the last 3D pass left bound. depth_test/
    // depth_write/depth_func are real, per-batch parameters (task #191) so
    // gl_segmented_rect_3d_tex()'s genuine 3D world-space callers (HUD
    // nametags/icons wanting real depth-testing) get their own correct
    // state instead of this always forcing DepthEnable=false - see the
    // pass-boundary flushPending() calls in llviewerdisplay.cpp/
    // llhudobject.cpp that keep depth-tested HUD batches from ever merging
    // with non-depth-tested screen-space UI batches.
    ID3D11DepthStencilState* ds = DXStateCache::getDepthStencilState(state.depthTest, state.depthWrite, state.depthFunc);
    ctx->OMSetDepthStencilState(ds, 0);

    ctx->Draw((UINT)count, 0);

    mVertices.erase(mVertices.begin(), mVertices.begin() + count);
}
