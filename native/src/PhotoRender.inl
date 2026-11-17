// GameThread state. Draw both surfaces through the same RenderWare queue used
// for the held model. The saved texture contains just the scene.
namespace photo_render {
using weapon_optic_render::Optic;

Optic preview;
Optic capture;
float screenHalfHeight{};
int imageQuarterTurns{};
double pressedAt{-1};
double readyAt{-1};
bool pending{};
bool rendered{};

void Request(const CamBasis& camera, float fov, bool flipHorizontal, bool flipVertical) {
    if (pending) {
        photocapture::ReportFailure("previous photograph is still rendering");
        return;
    }
    pressedAt = NowMs();
    readyAt = -1;
    pending = true;
    rendered = false;
    capture.frame.camera = camera;
    capture.tanFov = std::tan(fov * 3.14159265f / 360.f);
    capture.flipHorizontal = flipHorizontal;
    capture.flipVertical = flipVertical;
}

void ResetFrame() { preview.visible = false; }

bool NeedsRaster() {
    return (preview.visible && !preview.raster) || (pending && !capture.raster);
}

void Prepare(const RwMat& model) {
    if (FormalCutsceneRunning() || ScriptedCutsceneRunning() ||
        !BuildWeaponOpticFrame(weaponoptic::PhotoCamera, model, preview.frame))
        return;
    const auto profile = calib::SnapshotOptic(weaponoptic::PhotoCamera);
    screenHalfHeight = weaponoptic::PhotoHeightMm(profile) * .0005f;
    imageQuarterTurns = profile.imageQuarterTurns;
    preview.visible = true;
    preview.width = photoscreen::PreviewWidth;
    preview.height = photoscreen::PreviewHeight;
    preview.farClip = photoscreen::PreviewFarClip;
    preview.extendFog = true;
    preview.tanFov = std::tan(scopeaim::PhotoFovX(kVrFovX) * 3.14159265f / 360.f);
    preview.flipHorizontal = profile.value[8] != 0;
    preview.flipVertical = profile.value[9] != 0;
}

void PrepareRasters() {
    if (!rear_mirror_render::Resolve()) return;
    if (preview.visible && !preview.raster) {
        preview.raster = g.RwRasterCreate(preview.width, preview.height, 32, 5);
    }
    if (pending && !capture.raster) {
        capture.width = photoscreen::CaptureWidth;
        capture.height = photoscreen::CaptureHeight;
        capture.detailed = true;
        capture.farClip = photoscreen::CaptureFarClip;
        capture.raster = g.RwRasterCreate(capture.width, capture.height, 32, 5);
    }
}

void Render(void* scene, std::uintptr_t camera, int sequence) {
    const bool readbackBusy = photocapture::TextureBusy();
    if (pending && (NowMs() - pressedAt > 1500.0 ||
                    FormalCutsceneRunning() || ScriptedCutsceneRunning())) {
        pending = false;
        readyAt = NowMs();
        photocapture::ReportFailure("photograph render unavailable");
    }
    if (!rear_mirror_render::Resolve()) return;
    weapon_optic_render::RenderOne(preview, scene, camera, false, nullptr);
    if (!pending || readbackBusy) return;
    capture.visible = true;
    rendered = weapon_optic_render::RenderOne(
        capture, scene, camera, true, &water_monitor_render::RenderSceneEffects);
    if (!rendered) return;
    pending = false;
    readyAt = NowMs();
    photocapture::QueueReadback(sequence, EyeRasterTexId(capture.raster),
                                capture.width, capture.height,
                                capture.flipHorizontal, capture.flipVertical);
}

void Draw() {
    if (!preview.visible || !rear_mirror_render::Resolve()) return;
    constexpr int States[] = {1, 6, 8, 9, 12, 14, 20};
    std::uintptr_t saved[std::size(States)]{};
    for (std::size_t i = 0; i < std::size(States); ++i)
        g.RwRenderStateGet(States[i], &saved[i]);
    g.RwRenderStateSet(6, reinterpret_cast<void*>(1));
    g.RwRenderStateSet(8, reinterpret_cast<void*>(1));
    g.RwRenderStateSet(9, reinterpret_cast<void*>(2));
    g.RwRenderStateSet(12, nullptr);
    g.RwRenderStateSet(14, nullptr);
    g.RwRenderStateSet(20, reinterpret_cast<void*>(1));

    const auto& surface = preview.frame.surface;
    const float halfWidth = preview.frame.halfSize;
    const float displayAspect = imageQuarterTurns % 2 ? screenHalfHeight / halfWidth :
                                                       halfWidth / screenHalfHeight;
    const auto imageSize = photoscreen::ImageHalfExtents(displayAspect);
    const auto vertex = [&](float x, float y, float depth, std::uint32_t color, float u, float v) {
        const auto p = photoscreen::DisplayToMount({x, y}, imageQuarterTurns);
        return SkyBackdropVertex{
            surface.pos - surface.right * (p.x * halfWidth) + surface.up * (p.y * screenHalfHeight)
                + surface.forward * depth,
            {}, color, u, v
        };
    };
    const auto draw = [&](SkyBackdropVertex (&verts)[4], void* raster) {
        unsigned short indices[]{0, 1, 2, 0, 2, 3};
        g.RwRenderStateSet(1, raster);
        if (g.RwIm3DTransform(verts, 4, nullptr, 1u | 8u | 16u)) {
            g.RwIm3DRenderIndexedPrimitive(3, indices, 6);
            g.RwIm3DEnd();
        }
    };
    const auto rectangle = [&](float x0, float y0, float x1, float y1, float depth,
                               std::uint32_t color) {
        SkyBackdropVertex verts[]{
            vertex(x0, y0, depth, color, 0, 0), vertex(x1, y0, depth, color, 0, 0),
            vertex(x1, y1, depth, color, 0, 0), vertex(x0, y1, depth, color, 0, 0)
        };
        draw(verts, nullptr);
    };
    const auto image = [&](const Optic& optic, float scale, float offset, float depth) {
        const float width = scale * imageSize.x;
        const float height = scale * imageSize.y;
        const float left = std::max(-1.f, offset - width);
        const float right = std::min(1.f, offset + width);
        if (left >= right) return;
        const float sign = optic.flipHorizontal ? -1.f : 1.f;
        const float u0 = .5f + sign * (left - offset) / (2.f * width);
        const float u1 = .5f + sign * (right - offset) / (2.f * width);
        const float top = optic.flipVertical ? 1.f : 0.f;
        const std::uint32_t color = optic.imageReady ? 0xffffffffu : 0xff101010u;
        SkyBackdropVertex verts[]{
            vertex(left, -height, depth, color, u0, 1.f - top),
            vertex(right, -height, depth, color, u1, 1.f - top),
            vertex(right, height, depth, color, u1, top),
            vertex(left, height, depth, color, u0, top)
        };
        draw(verts, optic.imageReady ? optic.raster : nullptr);
    };

    const auto review = photoscreen::Sample(NowMs(), pressedAt, readyAt, rendered);
    if (imageSize.x < .999f || imageSize.y < .999f)
        rectangle(-1.f, -1.f, 1.f, 1.f, .0005f, 0xff101010u);
    if (!review.photo || review.scale < 1.f) image(preview, 1.f, 0.f, 0.f);
    if (review.photo) image(capture, review.scale, review.offset, -.0007f);

    if (review.shutter > 0.f) {
        const float radius = 1.414214f * (1.f - review.shutter);
        constexpr float Edge[8][2] = {
            {1, 0}, {1, 1}, {0, 1}, {-1, 1}, {-1, 0}, {-1, -1}, {0, -1}, {1, -1}
        };
        for (int i = 0; i < 8; ++i) {
            const auto& a = Edge[i];
            const auto& b = Edge[(i + 1) % 8];
            const float ra = std::min(1.f, radius / std::hypot(a[0], a[1]));
            const float rb = std::min(1.f, radius / std::hypot(b[0], b[1]));
            const std::uint32_t color = i % 2 ? 0xff17171au : 0xff101012u;
            constexpr float Z = -.0014f;
            SkyBackdropVertex blade[]{
                vertex(a[0], a[1], Z, color, 0, 0),
                vertex(b[0], b[1], Z, color, 0, 0),
                vertex(b[0] * rb, b[1] * rb, Z, color, 0, 0),
                vertex(a[0] * ra, a[1] * ra, Z, color, 0, 0)
            };
            draw(blade, nullptr);
        }
    }

    if (photocapture::FailureVisible()) {
        constexpr char Message[] = "SAVE FAILED";
        constexpr int Pixels = (sizeof(Message) - 1) * 5 * 7;
        constexpr float PixelWidth = .022f;
        const float pixelHeight = std::min(.028f, PixelWidth * displayAspect);
        constexpr float TextWidth = ((sizeof(Message) - 1) * 6 - 1) * PixelWidth;
        rectangle(-TextWidth * .5f - .08f, -.93f, TextWidth * .5f + .08f, -.61f,
                  -.0021f, 0xff24248cu);
        std::array<SkyBackdropVertex, Pixels * 4> text;
        std::array<unsigned short, Pixels * 6> indices;
        unsigned short count = 0;
        int indexCount = 0;
        for (int letter = 0; Message[letter]; ++letter) {
            const auto glyph = radio::LabelGlyph(Message[letter]);
            for (int row = 0; row < 7; ++row) for (int column = 0; column < 5; ++column) {
                if (!(glyph[row] & (1u << (4 - column)))) continue;
                const float x = -TextWidth * .5f + (letter * 6 + column) * PixelWidth;
                const float y = -.67f - row * pixelHeight;
                constexpr auto Ink = 0xffffffffu;
                constexpr float Depth = -.0028f;
                text[count] = vertex(x, y - pixelHeight, Depth, Ink, 0, 0);
                text[count + 1] = vertex(x + PixelWidth, y - pixelHeight, Depth, Ink, 0, 0);
                text[count + 2] = vertex(x + PixelWidth, y, Depth, Ink, 0, 0);
                text[count + 3] = vertex(x, y, Depth, Ink, 0, 0);
                for (int corner : {0, 1, 2, 0, 2, 3})
                    indices[indexCount++] = static_cast<unsigned short>(count + corner);
                count += 4;
            }
        }
        g.RwRenderStateSet(1, nullptr);
        if (g.RwIm3DTransform(text.data(), count, nullptr, 1u | 8u | 16u)) {
            g.RwIm3DRenderIndexedPrimitive(3, indices.data(), indexCount);
            g.RwIm3DEnd();
        }
    }
    for (std::size_t i = 0; i < std::size(States); ++i)
        g.RwRenderStateSet(States[i], reinterpret_cast<void*>(saved[i]));
}

} // namespace photo_render
