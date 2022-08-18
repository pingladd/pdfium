// Copyright 2019 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <utility>

#include "build/build_config.h"
#include "core/fxge/dib/fx_dib.h"
#include "public/fpdf_progressive.h"
#include "testing/embedder_test.h"
#include "testing/embedder_test_constants.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/base/check.h"

namespace {

constexpr FX_ARGB kBlack = 0xFF000000;
constexpr FX_ARGB kBlue = 0xFF0000FF;
constexpr FX_ARGB kGreen = 0xFF00FF00;
constexpr FX_ARGB kRed = 0xFFFF0000;
constexpr FX_ARGB kWhite = 0xFFFFFFFF;

const char* AnnotationStampWithApBaseContentChecksum() {
#if BUILDFLAG(IS_APPLE) && !defined(_SKIA_SUPPORT_) && \
    !defined(_SKIA_SUPPORT_PATHS_)
  static constexpr char kAnnotationStampWithApBaseContentChecksum[] =
      "243f3d6267d9db09198fed9f8c4957fd";
#else
  static constexpr char kAnnotationStampWithApBaseContentChecksum[] =
      "e31414933c9ff3950773981e5bf61678";
#endif

  return kAnnotationStampWithApBaseContentChecksum;
}

}  // namespace

class FPDFProgressiveRenderEmbedderTest : public EmbedderTest {
 public:
  class FakePause : public IFSDK_PAUSE {
   public:
    explicit FakePause(bool should_pause) : should_pause_(should_pause) {
      IFSDK_PAUSE::version = 1;
      IFSDK_PAUSE::user = nullptr;
      IFSDK_PAUSE::NeedToPauseNow = Pause_NeedToPauseNow;
    }
    ~FakePause() = default;
    static FPDF_BOOL Pause_NeedToPauseNow(IFSDK_PAUSE* param) {
      return static_cast<FakePause*>(param)->should_pause_;
    }

   private:
    const bool should_pause_;
  };

  // StartRenderPageWithFlags() with no flags.
  // The call returns true if the rendering is complete.
  bool StartRenderPage(FPDF_PAGE page, IFSDK_PAUSE* pause);

  // Start rendering of |page| into a bitmap with the ability to |pause| the
  // rendering with the specified rendering |flags|.
  // The call returns true if the rendering is complete.
  //
  // See public/fpdfview.h for a list of page rendering flags.
  bool StartRenderPageWithFlags(FPDF_PAGE page, IFSDK_PAUSE* pause, int flags);

  // Start rendering of |page| into a bitmap with the ability to pause the
  // rendering with the specified rendering |flags| and the specified
  // |color_scheme|. This also takes in the |background_color| for the bitmap.
  // The call returns true if the rendering is complete.
  //
  // See public/fpdfview.h for the list of page rendering flags and
  // the list of colors in the scheme.
  bool StartRenderPageWithColorSchemeAndBackground(
      FPDF_PAGE page,
      IFSDK_PAUSE* pause,
      int flags,
      const FPDF_COLORSCHEME* color_scheme,
      uint32_t background_color);

  // Continue rendering of |page| into the bitmap created in
  // StartRenderPageWithFlags().
  // The call returns true if the rendering is complete.
  bool ContinueRenderPage(FPDF_PAGE page, IFSDK_PAUSE* pause);

  // Simplified form of FinishRenderPageWithForms() with no form handle.
  ScopedFPDFBitmap FinishRenderPage(FPDF_PAGE page);

  // Finish rendering of |page| into the bitmap created in
  // StartRenderPageWithFlags(). This also renders the forms associated with
  // the page. The form handle associated with |page| should be passed in via
  // |handle|. If |handle| is nullptr, then forms on the page will not be
  // rendered.
  // This returns the bitmap generated by the progressive render calls.
  ScopedFPDFBitmap FinishRenderPageWithForms(FPDF_PAGE page,
                                             FPDF_FORMHANDLE handle);

  // Convert the |page| into a bitmap with a |background_color|, using the
  // color scheme render API with the specific |flags| and |color_scheme|.
  // The form handle associated with |page| should be passed in via |handle|.
  // If |handle| is nullptr, then forms on the page will not be rendered.
  // This returns the bitmap generated by the progressive render calls.
  //
  // See public/fpdfview.h for a list of page rendering flags and
  // the color scheme that can be applied for rendering.
  ScopedFPDFBitmap RenderPageWithForcedColorScheme(
      FPDF_PAGE page,
      FPDF_FORMHANDLE handle,
      int flags,
      const FPDF_COLORSCHEME* color_scheme,
      FX_ARGB background_color);

 protected:
  // Utility method to render the |page_num| of the currently loaded Pdf
  // using RenderPageWithForcedColorScheme() passing in the render options
  // and expected values for bitmap verification.
  void VerifyRenderingWithColorScheme(int page_num,
                                      int flags,
                                      const FPDF_COLORSCHEME* color_scheme,
                                      FX_ARGB background_color,
                                      int bitmap_width,
                                      int bitmap_height,
                                      const char* md5);

 private:
  // Keeps the bitmap used for progressive rendering alive until
  // FPDF_RenderPage_Close() is called after which the bitmap is returned
  // to the caller.
  ScopedFPDFBitmap progressive_render_bitmap_;
  int progressive_render_flags_ = 0;
};

bool FPDFProgressiveRenderEmbedderTest::StartRenderPage(FPDF_PAGE page,
                                                        IFSDK_PAUSE* pause) {
  return StartRenderPageWithFlags(page, pause, 0);
}

bool FPDFProgressiveRenderEmbedderTest::StartRenderPageWithFlags(
    FPDF_PAGE page,
    IFSDK_PAUSE* pause,
    int flags) {
  int width = static_cast<int>(FPDF_GetPageWidth(page));
  int height = static_cast<int>(FPDF_GetPageHeight(page));
  progressive_render_flags_ = flags;
  int alpha = FPDFPage_HasTransparency(page) ? 1 : 0;
  progressive_render_bitmap_ =
      ScopedFPDFBitmap(FPDFBitmap_Create(width, height, alpha));
  FPDF_DWORD fill_color = alpha ? 0x00000000 : 0xFFFFFFFF;
  FPDFBitmap_FillRect(progressive_render_bitmap_.get(), 0, 0, width, height,
                      fill_color);
  int rv = FPDF_RenderPageBitmap_Start(progressive_render_bitmap_.get(), page,
                                       0, 0, width, height, 0,
                                       progressive_render_flags_, pause);
  return rv != FPDF_RENDER_TOBECONTINUED;
}

bool FPDFProgressiveRenderEmbedderTest::
    StartRenderPageWithColorSchemeAndBackground(
        FPDF_PAGE page,
        IFSDK_PAUSE* pause,
        int flags,
        const FPDF_COLORSCHEME* color_scheme,
        uint32_t background_color) {
  int width = static_cast<int>(FPDF_GetPageWidth(page));
  int height = static_cast<int>(FPDF_GetPageHeight(page));
  progressive_render_flags_ = flags;
  int alpha = FPDFPage_HasTransparency(page) ? 1 : 0;
  progressive_render_bitmap_ =
      ScopedFPDFBitmap(FPDFBitmap_Create(width, height, alpha));
  DCHECK(progressive_render_bitmap_);
  FPDFBitmap_FillRect(progressive_render_bitmap_.get(), 0, 0, width, height,
                      background_color);
  int rv = FPDF_RenderPageBitmapWithColorScheme_Start(
      progressive_render_bitmap_.get(), page, 0, 0, width, height, 0,
      progressive_render_flags_, color_scheme, pause);
  return rv != FPDF_RENDER_TOBECONTINUED;
}

bool FPDFProgressiveRenderEmbedderTest::ContinueRenderPage(FPDF_PAGE page,
                                                           IFSDK_PAUSE* pause) {
  DCHECK(progressive_render_bitmap_);

  int rv = FPDF_RenderPage_Continue(page, pause);
  return rv != FPDF_RENDER_TOBECONTINUED;
}

ScopedFPDFBitmap FPDFProgressiveRenderEmbedderTest::FinishRenderPage(
    FPDF_PAGE page) {
  return FinishRenderPageWithForms(page, /*handle=*/nullptr);
}

ScopedFPDFBitmap FPDFProgressiveRenderEmbedderTest::FinishRenderPageWithForms(
    FPDF_PAGE page,
    FPDF_FORMHANDLE handle) {
  DCHECK(progressive_render_bitmap_);

  int width = static_cast<int>(FPDF_GetPageWidth(page));
  int height = static_cast<int>(FPDF_GetPageHeight(page));
  FPDF_FFLDraw(handle, progressive_render_bitmap_.get(), page, 0, 0, width,
               height, 0, progressive_render_flags_);
  FPDF_RenderPage_Close(page);
  return std::move(progressive_render_bitmap_);
}

ScopedFPDFBitmap
FPDFProgressiveRenderEmbedderTest::RenderPageWithForcedColorScheme(
    FPDF_PAGE page,
    FPDF_FORMHANDLE handle,
    int flags,
    const FPDF_COLORSCHEME* color_scheme,
    FX_ARGB background_color) {
  FakePause pause(true);
  bool render_done = StartRenderPageWithColorSchemeAndBackground(
                         page, &pause, flags, color_scheme, background_color) ==
                     FPDF_RENDER_TOBECONTINUED;
  EXPECT_FALSE(render_done);

  while (!render_done) {
    render_done = ContinueRenderPage(page, &pause);
  }
  return FinishRenderPageWithForms(page, form_handle());
}

TEST_F(FPDFProgressiveRenderEmbedderTest, RenderWithoutPause) {
  // Test rendering of page content using progressive render APIs
  // without pausing the rendering.
  ASSERT_TRUE(OpenDocument("annotation_stamp_with_ap.pdf"));
  FPDF_PAGE page = LoadPage(0);
  ASSERT_TRUE(page);
  FakePause pause(false);
  EXPECT_TRUE(StartRenderPage(page, &pause));
  ScopedFPDFBitmap bitmap = FinishRenderPage(page);
  CompareBitmap(bitmap.get(), 595, 842,
                AnnotationStampWithApBaseContentChecksum());
  UnloadPage(page);
}

TEST_F(FPDFProgressiveRenderEmbedderTest, RenderWithPause) {
  // Test rendering of page content using progressive render APIs
  // with pause in rendering.
  ASSERT_TRUE(OpenDocument("annotation_stamp_with_ap.pdf"));
  FPDF_PAGE page = LoadPage(0);
  ASSERT_TRUE(page);
  FakePause pause(true);
  bool render_done = StartRenderPage(page, &pause);
  EXPECT_FALSE(render_done);

  while (!render_done) {
    render_done = ContinueRenderPage(page, &pause);
  }
  ScopedFPDFBitmap bitmap = FinishRenderPage(page);
  CompareBitmap(bitmap.get(), 595, 842,
                AnnotationStampWithApBaseContentChecksum());
  UnloadPage(page);
}

TEST_F(FPDFProgressiveRenderEmbedderTest, RenderAnnotWithPause) {
  // Test rendering of the page with annotations using progressive render APIs
  // with pause in rendering.
  ASSERT_TRUE(OpenDocument("annotation_stamp_with_ap.pdf"));
  FPDF_PAGE page = LoadPage(0);
  ASSERT_TRUE(page);
  FakePause pause(true);
  bool render_done = StartRenderPageWithFlags(page, &pause, FPDF_ANNOT);
  EXPECT_FALSE(render_done);

  while (!render_done) {
    render_done = ContinueRenderPage(page, &pause);
  }
  ScopedFPDFBitmap bitmap = FinishRenderPage(page);
  CompareBitmap(bitmap.get(), 595, 842,
                pdfium::AnnotationStampWithApChecksum());
  UnloadPage(page);
}

TEST_F(FPDFProgressiveRenderEmbedderTest, RenderFormsWithPause) {
  // Test rendering of the page with forms using progressive render APIs
  // with pause in rendering.
  ASSERT_TRUE(OpenDocument("text_form.pdf"));
  FPDF_PAGE page = LoadPage(0);
  ASSERT_TRUE(page);
  FakePause pause(true);
  bool render_done = StartRenderPage(page, &pause);
  EXPECT_FALSE(render_done);

  while (!render_done) {
    render_done = ContinueRenderPage(page, &pause);
  }
  ScopedFPDFBitmap bitmap = FinishRenderPageWithForms(page, form_handle());
  CompareBitmap(bitmap.get(), 300, 300, pdfium::TextFormChecksum());
  UnloadPage(page);
}

void FPDFProgressiveRenderEmbedderTest::VerifyRenderingWithColorScheme(
    int page_num,
    int flags,
    const FPDF_COLORSCHEME* color_scheme,
    FX_ARGB background_color,
    int bitmap_width,
    int bitmap_height,
    const char* md5) {
  ASSERT_TRUE(document());

  FPDF_PAGE page = LoadPage(page_num);
  ASSERT_TRUE(page);

  ScopedFPDFBitmap bitmap = RenderPageWithForcedColorScheme(
      page, form_handle(), flags, color_scheme, background_color);
  ASSERT_TRUE(bitmap);
  CompareBitmap(bitmap.get(), bitmap_width, bitmap_height, md5);
  UnloadPage(page);
}

TEST_F(FPDFProgressiveRenderEmbedderTest, RenderTextWithColorScheme) {
// Test rendering of text with forced color scheme on.
#if defined(_SKIA_SUPPORT_)
  static constexpr char kContentWithTextChecksum[] =
      "5ece6059efdc2ecb2894fa3cf329dc94";
#elif BUILDFLAG(IS_APPLE) && !defined(_SKIA_SUPPORT_PATHS_)
  static constexpr char kContentWithTextChecksum[] =
      "ee4ec12f54ce8d117a73bd9b85a8954d";
#else
  static constexpr char kContentWithTextChecksum[] =
      "704db63ed2bf77254ecaa8035b85f21a";
#endif

  ASSERT_TRUE(OpenDocument("hello_world.pdf"));

  FPDF_COLORSCHEME color_scheme{kBlack, kWhite, kWhite, kWhite};
  VerifyRenderingWithColorScheme(/*page_num=*/0, /*flags=*/0, &color_scheme,
                                 kBlack, 200, 200, kContentWithTextChecksum);
}

TEST_F(FPDFProgressiveRenderEmbedderTest, RenderPathWithColorScheme) {
  // Test rendering of paths with forced color scheme on.
#if defined(_SKIA_SUPPORT_) || defined(_SKIA_SUPPORT_PATHS_)
  static constexpr char kRectanglesChecksum[] =
      "4b0f850a94698d07b6cd2814d1b4ccb7";
#else
  static constexpr char kRectanglesChecksum[] =
      "249f59b0d066c4f6bd89782a80822219";
#endif

  ASSERT_TRUE(OpenDocument("rectangles.pdf"));

  FPDF_COLORSCHEME color_scheme{kWhite, kRed, kBlue, kBlue};
  VerifyRenderingWithColorScheme(/*page_num=*/0, /*flags=*/0, &color_scheme,
                                 kBlack, 200, 300, kRectanglesChecksum);
}

TEST_F(FPDFProgressiveRenderEmbedderTest,
       RenderPathWithColorSchemeAndConvertFillToStroke) {
  // Test rendering of paths with forced color scheme on and conversion from
  // fill to stroke enabled. The fill paths should be rendered as stroke.
#if defined(_SKIA_SUPPORT_) || defined(_SKIA_SUPPORT_PATHS_)
  static constexpr char kRectanglesChecksum[] =
      "c1cbbd2ce6921f608a3c55140592419b";
#else
  static constexpr char kRectanglesChecksum[] =
      "0ebcc11e617635eca1fa9ce475383a80";
#endif

  ASSERT_TRUE(OpenDocument("rectangles.pdf"));

  FPDF_COLORSCHEME color_scheme{kWhite, kRed, kBlue, kBlue};
  VerifyRenderingWithColorScheme(/*page_num=*/0, FPDF_CONVERT_FILL_TO_STROKE,
                                 &color_scheme, kBlack, 200, 300,
                                 kRectanglesChecksum);
}

TEST_F(FPDFProgressiveRenderEmbedderTest, RenderHighlightWithColorScheme) {
// Test rendering of highlight with forced color scheme on.
//
// Note: The fill color rendered for highlight is different from the normal
// path since highlights have Multiply blend mode, while the other path has
// Normal blend mode.
#if defined(_SKIA_SUPPORT_)
  static constexpr char kContentWithHighlightFillChecksum[] =
      "9b6273fdbc9db780c49f7540756209f8";
#elif defined(_SKIA_SUPPORT_PATHS_)
  static constexpr char kContentWithHighlightFillChecksum[] =
      "1ad601278736432e2f82ea37ab6a28ba";
#else
#if BUILDFLAG(IS_APPLE)
  static constexpr char kContentWithHighlightFillChecksum[] =
      "a820afec9b99d3d3f2e9e9382bbad7c1";
#else
  static constexpr char kContentWithHighlightFillChecksum[] =
      "a08a0639f89446f66f3689ee8e08b9fe";
#endif  // BUILDFLAG(IS_APPLE)
#endif

  ASSERT_TRUE(OpenDocument("annotation_highlight_square_with_ap.pdf"));

  FPDF_COLORSCHEME color_scheme{kRed, kGreen, kWhite, kWhite};
  VerifyRenderingWithColorScheme(/*page_num=*/0, FPDF_ANNOT, &color_scheme,
                                 kBlue, 612, 792,
                                 kContentWithHighlightFillChecksum);
}

TEST_F(FPDFProgressiveRenderEmbedderTest,
       RenderHighlightWithColorSchemeAndConvertFillToStroke) {
  // Test rendering of highlight with forced color and converting fill to
  // stroke. The highlight should be rendered as a stroke of the rect.
  //
  // Note: The stroke color rendered for highlight is different from the normal
  // path since highlights have Multiply blend mode, while the other path has
  // Normal blend mode.

#if defined(_SKIA_SUPPORT_)
  static constexpr char kMD5ContentWithHighlight[] =
      "772246195d18f75d40a22bee913c098f";
#elif defined(_SKIA_SUPPORT_PATHS_)
  static constexpr char kMD5ContentWithHighlight[] =
      "eff6eef2409ef5fbf4612bf6af42e0a0";
#else
#if BUILDFLAG(IS_APPLE)
  static constexpr char kMD5ContentWithHighlight[] =
      "8837bea0b3520164b1784e513c882a2d";
#else
  static constexpr char kMD5ContentWithHighlight[] =
      "3dd8c02f5c06bac85e0d2c8bf37d1dc4";
#endif  // BUILDFLAG(IS_APPLE)
#endif

  ASSERT_TRUE(OpenDocument("annotation_highlight_square_with_ap.pdf"));

  FPDF_COLORSCHEME color_scheme{kRed, kGreen, kWhite, kWhite};
  VerifyRenderingWithColorScheme(
      /*page_num=*/0, FPDF_ANNOT | FPDF_CONVERT_FILL_TO_STROKE, &color_scheme,
      kBlue, 612, 792, kMD5ContentWithHighlight);
}

TEST_F(FPDFProgressiveRenderEmbedderTest, RenderInkWithColorScheme) {
// Test rendering of multiple ink with forced color scheme on.
#if defined(_SKIA_SUPPORT_) || defined(_SKIA_SUPPORT_PATHS_)
  static constexpr char kContentWithInkChecksum[] =
      "ebc57721e4c8da34156e09b9b2e62fb0";
#else
  static constexpr char kContentWithInkChecksum[] =
      "797bce7dc6c50ee86b095405df9fe5aa";
#endif  // defined(_SKIA_SUPPORT_) || defined(_SKIA_SUPPORT_PATHS_)

  ASSERT_TRUE(OpenDocument("annotation_ink_multiple.pdf"));

  FPDF_COLORSCHEME color_scheme{kBlack, kGreen, kRed, kRed};
  VerifyRenderingWithColorScheme(/*page_num=*/0, FPDF_ANNOT, &color_scheme,
                                 kBlack, 612, 792, kContentWithInkChecksum);
}

TEST_F(FPDFProgressiveRenderEmbedderTest, RenderStampWithColorScheme) {
// Test rendering of static annotation with forced color scheme on.
#if defined(_SKIA_SUPPORT_) || defined(_SKIA_SUPPORT_PATHS_)
  static constexpr char kContentWithStampChecksum[] =
      "a791fdb4f595bb6c4187cc2aeed5e9e8";
#elif BUILDFLAG(IS_APPLE)
  static constexpr char kContentWithStampChecksum[] =
      "7a209e29caeeab7d2b25b34570a4ace6";
#else
  static constexpr char kContentWithStampChecksum[] =
      "3bbbfc6cc18801906285a232c4a20617";
#endif

  ASSERT_TRUE(OpenDocument("annotation_stamp_with_ap.pdf"));

  FPDF_COLORSCHEME color_scheme{kBlue, kGreen, kRed, kRed};
  VerifyRenderingWithColorScheme(/*page_num=*/0, FPDF_ANNOT, &color_scheme,
                                 kWhite, 595, 842, kContentWithStampChecksum);
}

TEST_F(FPDFProgressiveRenderEmbedderTest, RenderFormWithColorScheme) {
  // Test rendering of form does not change with forced color scheme on.
#if defined(_SKIA_SUPPORT_) || defined(_SKIA_SUPPORT_PATHS_)
  static constexpr char kContentWithFormChecksum[] =
      "9f75d98afc6d6313bd87e6562ea6df15";
#else
  static constexpr char kContentWithFormChecksum[] =
      "080f7a4381606659301440e1b14dca35";
#endif

  ASSERT_TRUE(OpenDocument("annotiter.pdf"));

  FPDF_COLORSCHEME color_scheme{kGreen, kGreen, kRed, kRed};
  VerifyRenderingWithColorScheme(/*page_num=*/0, FPDF_ANNOT, &color_scheme,
                                 kWhite, 612, 792, kContentWithFormChecksum);

  // Verify that the MD5 hash matches when rendered without |color_scheme|.
  VerifyRenderingWithColorScheme(/*page_num=*/0, FPDF_ANNOT,
                                 /*color_scheme=*/nullptr, kWhite, 612, 792,
                                 kContentWithFormChecksum);
}
