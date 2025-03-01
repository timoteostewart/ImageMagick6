/*
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
%                                                                             %
%                                                                             %
%                                                                             %
%         AAA   TTTTT  TTTTT  RRRR   IIIII  BBBB   U   U  TTTTT  EEEEE        %
%        A   A    T      T    R   R    I    B   B  U   U    T    E            %
%        AAAAA    T      T    RRRR     I    BBBB   U   U    T    EEE          %
%        A   A    T      T    R R      I    B   B  U   U    T    E            %
%        A   A    T      T    R  R   IIIII  BBBB    UUU     T    EEEEE        %
%                                                                             %
%                                                                             %
%                    MagickCore Get / Set Image Attributes                    %
%                                                                             %
%                              Software Design                                %
%                                   Cristy                                    %
%                                October 2002                                 %
%                                                                             %
%                                                                             %
%  Copyright 1999 ImageMagick Studio LLC, a non-profit organization           %
%  dedicated to making software imaging solutions freely available.           %
%                                                                             %
%  You may not use this file except in compliance with the License.  You may  %
%  obtain a copy of the License at                                            %
%                                                                             %
%    https://imagemagick.org/script/license.php                               %
%                                                                             %
%  Unless required by applicable law or agreed to in writing, software        %
%  distributed under the License is distributed on an "AS IS" BASIS,          %
%  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.   %
%  See the License for the specific language governing permissions and        %
%  limitations under the License.                                             %
%                                                                             %
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
%
%
%
*/

/*
  Include declarations.
*/
#include "magick/studio.h"
#include "magick/artifact.h"
#include "magick/attribute.h"
#include "magick/blob.h"
#include "magick/blob-private.h"
#include "magick/cache.h"
#include "magick/cache-private.h"
#include "magick/cache-view.h"
#include "magick/client.h"
#include "magick/channel.h"
#include "magick/color.h"
#include "magick/color-private.h"
#include "magick/colormap.h"
#include "magick/colormap-private.h"
#include "magick/colorspace.h"
#include "magick/colorspace-private.h"
#include "magick/composite.h"
#include "magick/composite-private.h"
#include "magick/constitute.h"
#include "magick/deprecate.h"
#include "magick/draw.h"
#include "magick/draw-private.h"
#include "magick/effect.h"
#include "magick/enhance.h"
#include "magick/exception.h"
#include "magick/exception-private.h"
#include "magick/geometry.h"
#include "magick/histogram.h"
#include "magick/identify.h"
#include "magick/image.h"
#include "magick/image-private.h"
#include "magick/list.h"
#include "magick/log.h"
#include "magick/memory_.h"
#include "magick/magick.h"
#include "magick/monitor.h"
#include "magick/monitor-private.h"
#include "magick/option.h"
#include "magick/paint.h"
#include "magick/pixel.h"
#include "magick/pixel-private.h"
#include "magick/property.h"
#include "magick/quantize.h"
#include "magick/random_.h"
#include "magick/resource_.h"
#include "magick/semaphore.h"
#include "magick/segment.h"
#include "magick/splay-tree.h"
#include "magick/string_.h"
#include "magick/string-private.h"
#include "magick/thread-private.h"
#include "magick/threshold.h"
#include "magick/transform.h"
#include "magick/utility.h"

/*
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
%                                                                             %
%                                                                             %
%                                                                             %
+   G e t I m a g e B o u n d i n g B o x                                     %
%                                                                             %
%                                                                             %
%                                                                             %
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
%
%  GetImageBoundingBox() returns the bounding box of an image canvas.
%
%  The format of the GetImageBoundingBox method is:
%
%      RectangleInfo GetImageBoundingBox(const Image *image,
%        ExceptionInfo *exception)
%
%  A description of each parameter follows:
%
%    o bounds: Method GetImageBoundingBox returns the bounding box of an
%      image canvas.
%
%    o image: the image.
%
%    o exception: return any errors or warnings in this structure.
%
*/

typedef struct _CensusInfo
{
  double
    left,
    right,
    top,
    bottom;
} CensusInfo;

static double GetEdgeBackgroundCensus(const Image *image,
  const CacheView *image_view,const GravityType gravity,const size_t width,
  const size_t height,const ssize_t x_offset,const ssize_t y_offset,
  ExceptionInfo *exception)
{
  CacheView
    *edge_view;

  const char
    *artifact;

  const PixelPacket
    *p;

  double
    census;

  Image
    *edge_image;

  MagickPixelPacket
    background,
    pixel;

  RectangleInfo
    edge_geometry;

  ssize_t
    y;

  /*
    Determine the percent of image background for this edge.
  */
  switch (gravity)
  {
    case NorthWestGravity:
    case NorthGravity:
    default:
    {
      p=GetCacheViewVirtualPixels(image_view,0,0,1,1,exception);
      break;
    }
    case NorthEastGravity:
    case EastGravity:
    {
      p=GetCacheViewVirtualPixels(image_view,(ssize_t) image->columns-1,0,1,1,
        exception);
      break;
    }
    case SouthEastGravity:
    case SouthGravity:
    {
      p=GetCacheViewVirtualPixels(image_view,(ssize_t) image->columns-1,
        (ssize_t) image->rows-1,1,1,exception);
      break;
    }
    case SouthWestGravity:
    case WestGravity:
    {
      p=GetCacheViewVirtualPixels(image_view,0,(ssize_t) image->rows-1,1,1,
        exception);
      break;
    }
  }
  if (p == (const PixelPacket *) NULL)
    return(0.0);
  GetMagickPixelPacket(image,&background);
  SetMagickPixelPacket(image,p,(IndexPacket *) NULL,&background);
  artifact=GetImageArtifact(image,"background");
  if (artifact != (const char *) NULL)
    (void) QueryMagickColor(artifact,&background,exception);
  artifact=GetImageArtifact(image,"trim:background-color");
  if (artifact != (const char *) NULL)
    (void) QueryMagickColor(artifact,&background,exception);
  edge_geometry.width=width;
  edge_geometry.height=height;
  edge_geometry.x=x_offset;
  edge_geometry.y=y_offset;
  GravityAdjustGeometry(image->columns,image->rows,gravity,&edge_geometry);
  edge_image=CropImage(image,&edge_geometry,exception);
  if (edge_image == (Image *) NULL)
    return(0.0);
  census=0.0;
  GetMagickPixelPacket(edge_image,&pixel);
  edge_view=AcquireVirtualCacheView(edge_image,exception);
  for (y=0; y < (ssize_t) edge_image->rows; y++)
  {
    ssize_t
      x;

    p=GetCacheViewVirtualPixels(edge_view,0,y,edge_image->columns,1,exception);
    if (p == (const PixelPacket *) NULL)
      break;
    for (x=0; x < (ssize_t) edge_image->columns; x++)
    {
      SetMagickPixelPacket(edge_image,p,(IndexPacket *) NULL,&pixel);
      if (IsMagickColorSimilar(&pixel,&background) == MagickFalse)
        census++;
      p++;
    }
  }
  census/=((double) edge_image->columns*edge_image->rows);
  edge_view=DestroyCacheView(edge_view);
  edge_image=DestroyImage(edge_image);
  return(census);
}

static inline double GetMinEdgeBackgroundCensus(const CensusInfo *edge)
{
  double
    census;

  census=MagickMin(MagickMin(MagickMin(edge->left,edge->right),edge->top),
    edge->bottom);
  return(census);
}

static RectangleInfo GetEdgeBoundingBox(const Image *image,
  ExceptionInfo *exception)
{
  CacheView
    *edge_view;

  CensusInfo
    edge,
    vertex;

  const char
    *artifact;

  double
    background_census,
    percent_background;

  Image
    *edge_image;

  RectangleInfo
    bounds;

  /*
    Get the image bounding box.
  */
  assert(image != (Image *) NULL);
  assert(image->signature == MagickCoreSignature);
  if (IsEventLogging() != MagickFalse)
    (void) LogMagickEvent(TraceEvent,GetMagickModule(),"%s",image->filename);
  SetGeometry(image,&bounds);
  edge_image=CloneImage(image,0,0,MagickTrue,exception);
  if (edge_image == (Image *) NULL)
    return(bounds);
  (void) ParseAbsoluteGeometry("0x0+0+0",&edge_image->page);
  (void) memset(&vertex,0,sizeof(vertex));
  edge_view=AcquireVirtualCacheView(edge_image,exception);
  edge.left=GetEdgeBackgroundCensus(edge_image,edge_view,WestGravity,
    1,0,0,0,exception);
  edge.right=GetEdgeBackgroundCensus(edge_image,edge_view,EastGravity,
    1,0,0,0,exception);
  edge.top=GetEdgeBackgroundCensus(edge_image,edge_view,NorthGravity,
    0,1,0,0,exception);
  edge.bottom=GetEdgeBackgroundCensus(edge_image,edge_view,SouthGravity,
    0,1,0,0,exception);
  percent_background=1.0;
  artifact=GetImageArtifact(edge_image,"trim:percent-background");
  if (artifact != (const char *) NULL)
    percent_background=StringToDouble(artifact,(char **) NULL)/100.0;
  percent_background=MagickMin(MagickMax(1.0-percent_background,MagickEpsilon),
    1.0);
  background_census=GetMinEdgeBackgroundCensus(&edge);
  for ( ; background_census < percent_background;
          background_census=GetMinEdgeBackgroundCensus(&edge))
  {
    if ((bounds.width == 0) || (bounds.height == 0))
      break;
    if (fabs(edge.left-background_census) < MagickEpsilon)
      {
        /*
          Trim left edge.
        */
        vertex.left++;
        bounds.width--;
        edge.left=GetEdgeBackgroundCensus(edge_image,edge_view,
          NorthWestGravity,1,bounds.height,(ssize_t) vertex.left,(ssize_t)
          vertex.top,exception);
        edge.top=GetEdgeBackgroundCensus(edge_image,edge_view,
          NorthWestGravity,bounds.width,1,(ssize_t) vertex.left,(ssize_t)
          vertex.top,exception);
        edge.bottom=GetEdgeBackgroundCensus(edge_image,edge_view,
          SouthWestGravity,bounds.width,1,(ssize_t) vertex.left,(ssize_t)
          vertex.bottom,exception);
        continue;
      }
    if (fabs(edge.right-background_census) < MagickEpsilon)
      {
        /*
          Trim right edge.
        */
        vertex.right++;
        bounds.width--;
        edge.right=GetEdgeBackgroundCensus(edge_image,edge_view,
          NorthEastGravity,1,bounds.height,(ssize_t) vertex.right,(ssize_t)
          vertex.top,exception);
        edge.top=GetEdgeBackgroundCensus(edge_image,edge_view,
          NorthWestGravity,bounds.width,1,(ssize_t) vertex.left,(ssize_t)
          vertex.top,exception);
        edge.bottom=GetEdgeBackgroundCensus(edge_image,edge_view,
          SouthWestGravity,bounds.width,1,(ssize_t) vertex.left,(ssize_t)
          vertex.bottom,exception);
        continue;
      }
    if (fabs(edge.top-background_census) < MagickEpsilon)
      {
        /*
          Trim top edge.
        */
        vertex.top++;
        bounds.height--;
        edge.left=GetEdgeBackgroundCensus(edge_image,edge_view,
          NorthWestGravity,1,bounds.height,(ssize_t) vertex.left,(ssize_t)
          vertex.top,exception);
        edge.right=GetEdgeBackgroundCensus(edge_image,edge_view,
          NorthEastGravity,1,bounds.height,(ssize_t) vertex.right,(ssize_t)
          vertex.top,exception);
        edge.top=GetEdgeBackgroundCensus(edge_image,edge_view,
          NorthWestGravity,bounds.width,1,(ssize_t) vertex.left,(ssize_t)
          vertex.top,exception);
        continue;
      }
    if (fabs(edge.bottom-background_census) < MagickEpsilon)
      {
        /*
          Trim bottom edge.
        */
        vertex.bottom++;
        bounds.height--;
        edge.left=GetEdgeBackgroundCensus(edge_image,edge_view,
          NorthWestGravity,1,bounds.height,(ssize_t) vertex.left,(ssize_t)
          vertex.top,exception);
        edge.right=GetEdgeBackgroundCensus(edge_image,edge_view,
          NorthEastGravity,1,bounds.height,(ssize_t) vertex.right,(ssize_t)
          vertex.top,exception);
        edge.bottom=GetEdgeBackgroundCensus(edge_image,edge_view,
          SouthWestGravity,bounds.width,1,(ssize_t) vertex.left,(ssize_t)
          vertex.bottom,exception);
        continue;
      }
  }
  edge_view=DestroyCacheView(edge_view);
  edge_image=DestroyImage(edge_image);
  bounds.x=(ssize_t) vertex.left;
  bounds.y=(ssize_t) vertex.top;
  if ((bounds.width == 0) || (bounds.height == 0))
    (void) ThrowMagickException(exception,GetMagickModule(),OptionWarning,
      "GeometryDoesNotContainImage","`%s'",image->filename);
  return(bounds);
}

MagickExport RectangleInfo GetImageBoundingBox(const Image *image,
  ExceptionInfo *exception)
{
  CacheView
    *image_view;

  const char
    *artifact;

  MagickBooleanType
    status;

  MagickPixelPacket
    target[4],
    zero;

  RectangleInfo
    bounds;

  const PixelPacket
    *p;

  ssize_t
    y;

  assert(image != (Image *) NULL);
  assert(image->signature == MagickCoreSignature);
  if (IsEventLogging() != MagickFalse)
    (void) LogMagickEvent(TraceEvent,GetMagickModule(),"%s",image->filename);
  artifact=GetImageArtifact(image,"trim:percent-background");
  if (artifact != (const char *) NULL)
    return(GetEdgeBoundingBox(image,exception));
  bounds.width=image->columns == 1 ? 1 : 0;
  bounds.height=image->rows == 1 ? 1 : 0;
  bounds.x=(ssize_t) image->columns;
  bounds.y=(ssize_t) image->rows;
  GetMagickPixelPacket(image,&target[0]);
  image_view=AcquireVirtualCacheView(image,exception);
  p=GetCacheViewVirtualPixels(image_view,0,0,1,1,exception);
  if (p == (const PixelPacket *) NULL)
    {
      image_view=DestroyCacheView(image_view);
      return(bounds);
    }
  SetMagickPixelPacket(image,p,GetCacheViewVirtualIndexQueue(image_view),
    &target[0]);
  GetMagickPixelPacket(image,&target[1]);
  p=GetCacheViewVirtualPixels(image_view,(ssize_t) image->columns-1,0,1,1,
    exception);
  if (p != (const PixelPacket *) NULL)
    SetMagickPixelPacket(image,p,GetCacheViewVirtualIndexQueue(image_view),
      &target[1]);
  GetMagickPixelPacket(image,&target[2]);
  p=GetCacheViewVirtualPixels(image_view,0,(ssize_t) image->rows-1,1,1,
    exception);
  if (p != (const PixelPacket *) NULL)
    SetMagickPixelPacket(image,p,GetCacheViewVirtualIndexQueue(image_view),
      &target[2]);
  GetMagickPixelPacket(image,&target[3]);
  p=GetCacheViewVirtualPixels(image_view,(ssize_t) image->columns-1,
    (ssize_t) image->rows-1,1,1,exception);
  if (p != (const PixelPacket *) NULL)
    SetMagickPixelPacket(image,p,GetCacheViewVirtualIndexQueue(image_view),
      &target[3]);
  status=MagickTrue;
  GetMagickPixelPacket(image,&zero);
#if defined(MAGICKCORE_OPENMP_SUPPORT)
  #pragma omp parallel for schedule(static) shared(status) \
    magick_number_threads(image,image,image->rows,2)
#endif
  for (y=0; y < (ssize_t) image->rows; y++)
  {
    MagickPixelPacket
      pixel;

    RectangleInfo
      bounding_box;

    const IndexPacket
      *magick_restrict indexes;

    const PixelPacket
      *magick_restrict p;

    ssize_t
      x;

    if (status == MagickFalse)
      continue;
#if defined(MAGICKCORE_OPENMP_SUPPORT)
#   pragma omp critical (MagickCore_GetImageBoundingBox)
#endif
    bounding_box=bounds;
    p=GetCacheViewVirtualPixels(image_view,0,y,image->columns,1,exception);
    if (p == (const PixelPacket *) NULL)
      {
        status=MagickFalse;
        continue;
      }
    indexes=GetCacheViewVirtualIndexQueue(image_view);
    pixel=zero;
    for (x=0; x < (ssize_t) image->columns; x++)
    {
      SetMagickPixelPacket(image,p,indexes+x,&pixel);
      if ((x < bounding_box.x) &&
          (IsMagickColorSimilar(&pixel,&target[0]) == MagickFalse))
        bounding_box.x=x;
      if ((x > (ssize_t) bounding_box.width) &&
          (IsMagickColorSimilar(&pixel,&target[1]) == MagickFalse))
        bounding_box.width=(size_t) x;
      if ((y < bounding_box.y) &&
          (IsMagickColorSimilar(&pixel,&target[0]) == MagickFalse))
        bounding_box.y=y;
      if ((y > (ssize_t) bounding_box.height) &&
          (IsMagickColorSimilar(&pixel,&target[2]) == MagickFalse))
        bounding_box.height=(size_t) y;
      if ((x < (ssize_t) bounding_box.width) &&
          (y > (ssize_t) bounding_box.height) &&
          (IsMagickColorSimilar(&pixel,&target[3]) == MagickFalse))
        {
          bounding_box.width=(size_t) x;
          bounding_box.height=(size_t) y;
        }
      p++;
    }
#if defined(MAGICKCORE_OPENMP_SUPPORT)
#   pragma omp critical (MagickCore_GetImageBoundingBox)
#endif
    {
      if (bounding_box.x < bounds.x)
        bounds.x=bounding_box.x;
      if (bounding_box.y < bounds.y)
        bounds.y=bounding_box.y;
      if (bounding_box.width > bounds.width)
        bounds.width=bounding_box.width;
      if (bounding_box.height > bounds.height)
        bounds.height=bounding_box.height;
    }
  }
  image_view=DestroyCacheView(image_view);
  if ((bounds.width == 0) || (bounds.height == 0))
    (void) ThrowMagickException(exception,GetMagickModule(),OptionWarning,
      "GeometryDoesNotContainImage","`%s'",image->filename);
  else
    {
      bounds.width-=(size_t) (bounds.x-1);
      bounds.height-=(size_t) (bounds.y-1);
    }
  return(bounds);
}

/*
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
%                                                                             %
%                                                                             %
%                                                                             %
%   G e t I m a g e C h a n n e l D e p t h                                   %
%                                                                             %
%                                                                             %
%                                                                             %
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
%
%  GetImageChannelDepth() returns the depth of a particular image channel.
%
%  The format of the GetImageChannelDepth method is:
%
%      size_t GetImageDepth(const Image *image,ExceptionInfo *exception)
%      size_t GetImageChannelDepth(const Image *image,
%        const ChannelType channel,ExceptionInfo *exception)
%
%  A description of each parameter follows:
%
%    o image: the image.
%
%    o channel: the channel.
%
%    o exception: return any errors or warnings in this structure.
%
*/
MagickExport size_t GetImageDepth(const Image *image,ExceptionInfo *exception)
{
  return(GetImageChannelDepth(image,CompositeChannels,exception));
}

MagickExport size_t GetImageChannelDepth(const Image *image,
  const ChannelType channel,ExceptionInfo *exception)
{
  CacheView
    *image_view;

  MagickBooleanType
    status;

  ssize_t
    i;

  size_t
    *current_depth,
    depth,
    number_threads;

  ssize_t
    y;

  /*
    Compute image depth.
  */
  assert(image != (Image *) NULL);

  assert(image->signature == MagickCoreSignature);
  if (IsEventLogging() != MagickFalse)
    (void) LogMagickEvent(TraceEvent,GetMagickModule(),"%s",image->filename);
  number_threads=(size_t) GetMagickResourceLimit(ThreadResource);
  current_depth=(size_t *) AcquireQuantumMemory(number_threads,
    sizeof(*current_depth));
  if (current_depth == (size_t *) NULL)
    ThrowFatalException(ResourceLimitFatalError,"MemoryAllocationFailed");
  status=MagickTrue;
  for (i=0; i < (ssize_t) number_threads; i++)
    current_depth[i]=1;
  if ((image->storage_class == PseudoClass) && (image->matte == MagickFalse))
    {
#if defined(MAGICKCORE_OPENMP_SUPPORT)
      #pragma omp parallel for schedule(static) shared(status) \
        magick_number_threads(image,image,image->colors,1)
#endif
      for (i=0; i < (ssize_t) image->colors; i++)
      {
        const int
          id = GetOpenMPThreadId();

        while (current_depth[id] < MAGICKCORE_QUANTUM_DEPTH)
        {
          MagickBooleanType
            atDepth;

          QuantumAny
            range;

          atDepth=MagickTrue;
          range=GetQuantumRange(current_depth[id]);
          if ((channel & RedChannel) != 0)
            if (IsPixelAtDepth(image->colormap[i].red,range) == MagickFalse)
              atDepth=MagickFalse;
          if ((atDepth != MagickFalse) && ((channel & GreenChannel) != 0))
            if (IsPixelAtDepth(image->colormap[i].green,range) == MagickFalse)
              atDepth=MagickFalse;
          if ((atDepth != MagickFalse) && ((channel & BlueChannel) != 0))
            if (IsPixelAtDepth(image->colormap[i].blue,range) == MagickFalse)
              atDepth=MagickFalse;
          if ((atDepth != MagickFalse))
            break;
          current_depth[id]++;
        }
      }
      depth=current_depth[0];
      for (i=1; i < (ssize_t) number_threads; i++)
        if (depth < current_depth[i])
          depth=current_depth[i];
      current_depth=(size_t *) RelinquishMagickMemory(current_depth);
      return(depth);
    }
  image_view=AcquireVirtualCacheView(image,exception);
#if !defined(MAGICKCORE_HDRI_SUPPORT)
DisableMSCWarning(4127)
  if (1UL*QuantumRange <= MaxMap)
RestoreMSCWarning
    {
      size_t
        *depth_map;

      /*
        Scale pixels to desired (optimized with depth map).
      */
      depth_map=(size_t *) AcquireQuantumMemory(MaxMap+1,sizeof(*depth_map));
      if (depth_map == (size_t *) NULL)
        ThrowFatalException(ResourceLimitFatalError,"MemoryAllocationFailed");
      for (i=0; i <= (ssize_t) MaxMap; i++)
      {
        unsigned int
          depth;

        for (depth=1; depth < MAGICKCORE_QUANTUM_DEPTH; depth++)
        {
          Quantum
            pixel;

          QuantumAny
            range;

          range=GetQuantumRange(depth);
          pixel=(Quantum) i;
          if (pixel == ScaleAnyToQuantum(ScaleQuantumToAny(pixel,range),range))
            break;
        }
        depth_map[i]=depth;
      }
#if defined(MAGICKCORE_OPENMP_SUPPORT)
      #pragma omp parallel for schedule(static) shared(status) \
        magick_number_threads(image,image,image->rows,1)
#endif
      for (y=0; y < (ssize_t) image->rows; y++)
      {
        const int
          id = GetOpenMPThreadId();

        const IndexPacket
          *magick_restrict indexes;

        const PixelPacket
          *magick_restrict p;

        ssize_t
          x;

        if (status == MagickFalse)
          continue;
        p=GetCacheViewVirtualPixels(image_view,0,y,image->columns,1,exception);
        if (p == (const PixelPacket *) NULL)
          continue;
        indexes=GetCacheViewVirtualIndexQueue(image_view);
        for (x=0; x < (ssize_t) image->columns; x++)
        {
          Quantum
            pixel;

          if ((channel & RedChannel) != 0)
            {
              pixel=GetPixelRed(p);
              if (depth_map[ScaleQuantumToMap(pixel)] > current_depth[id])
                current_depth[id]=depth_map[ScaleQuantumToMap(pixel)];
            }
          if ((channel & GreenChannel) != 0)
            {
              pixel=GetPixelGreen(p);
              if (depth_map[ScaleQuantumToMap(pixel)] > current_depth[id])
                current_depth[id]=depth_map[ScaleQuantumToMap(pixel)];
            }
          if ((channel & BlueChannel) != 0)
            {
              pixel=GetPixelBlue(p);
              if (depth_map[ScaleQuantumToMap(pixel)] > current_depth[id])
                current_depth[id]=depth_map[ScaleQuantumToMap(pixel)];
            }
          if (((channel & OpacityChannel) != 0) &&
               (image->matte != MagickFalse))
            {
              pixel=GetPixelOpacity(p);
              if (depth_map[ScaleQuantumToMap(pixel)] > current_depth[id])
                current_depth[id]=depth_map[ScaleQuantumToMap(pixel)];
            }
          if (((channel & IndexChannel) != 0) &&
              (image->colorspace == CMYKColorspace))
            {
              pixel=GetPixelIndex(indexes+x);
              if (depth_map[ScaleQuantumToMap(pixel)] > current_depth[id])
                current_depth[id]=depth_map[ScaleQuantumToMap(pixel)];
            }
          p++;
        }
        if (current_depth[id] == MAGICKCORE_QUANTUM_DEPTH)
          status=MagickFalse;
      }
      image_view=DestroyCacheView(image_view);
      depth=current_depth[0];
      for (i=1; i < (ssize_t) number_threads; i++)
        if (depth < current_depth[i])
          depth=current_depth[i];
      depth_map=(size_t *) RelinquishMagickMemory(depth_map);
      current_depth=(size_t *) RelinquishMagickMemory(current_depth);
      return(depth);
    }
#endif
#if defined(MAGICKCORE_OPENMP_SUPPORT)
  #pragma omp parallel for schedule(static) shared(status) \
    magick_number_threads(image,image,image->rows,1)
#endif
  for (y=0; y < (ssize_t) image->rows; y++)
  {
    const int
      id = GetOpenMPThreadId();

    const IndexPacket
      *magick_restrict indexes;

    const PixelPacket
      *magick_restrict p;

    ssize_t
      x;

    if (status == MagickFalse)
      continue;
    p=GetCacheViewVirtualPixels(image_view,0,y,image->columns,1,exception);
    if (p == (const PixelPacket *) NULL)
      continue;
    indexes=GetCacheViewVirtualIndexQueue(image_view);
    for (x=0; x < (ssize_t) image->columns; x++)
    {
      while (current_depth[id] < MAGICKCORE_QUANTUM_DEPTH)
      {
        MagickBooleanType
          atDepth;

        QuantumAny
          range;

        atDepth=MagickTrue;
        range=GetQuantumRange(current_depth[id]);
        if ((atDepth != MagickFalse) && ((channel & RedChannel) != 0))
          if (IsPixelAtDepth(GetPixelRed(p),range) == MagickFalse)
            atDepth=MagickFalse;
        if ((atDepth != MagickFalse) && ((channel & GreenChannel) != 0))
          if (IsPixelAtDepth(GetPixelGreen(p),range) == MagickFalse)
            atDepth=MagickFalse;
        if ((atDepth != MagickFalse) && ((channel & BlueChannel) != 0))
          if (IsPixelAtDepth(GetPixelBlue(p),range) == MagickFalse)
            atDepth=MagickFalse;
        if ((atDepth != MagickFalse) && ((channel & OpacityChannel) != 0) &&
             (image->matte != MagickFalse))
          if (IsPixelAtDepth(GetPixelOpacity(p),range) == MagickFalse)
            atDepth=MagickTrue;
        if ((atDepth != MagickFalse) && ((channel & IndexChannel) != 0) &&
             (image->colorspace == CMYKColorspace))
          if (IsPixelAtDepth(GetPixelIndex(indexes+x),range) == MagickFalse)
            atDepth=MagickFalse;
        if ((atDepth != MagickFalse))
          break;
        current_depth[id]++;
      }
      p++;
    }
    if (current_depth[id] == MAGICKCORE_QUANTUM_DEPTH)
      status=MagickFalse;
  }
  image_view=DestroyCacheView(image_view);
  depth=current_depth[0];
  for (i=1; i < (ssize_t) number_threads; i++)
    if (depth < current_depth[i])
      depth=current_depth[i];
  current_depth=(size_t *) RelinquishMagickMemory(current_depth);
  return(depth);
}

/*
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
%                                                                             %
%                                                                             %
%                                                                             %
%   G e t I m a g e Q u a n t u m D e p t h                                   %
%                                                                             %
%                                                                             %
%                                                                             %
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
%
%  GetImageQuantumDepth() returns the depth of the image rounded to a legal
%  quantum depth: 8, 16, or 32.
%
%  The format of the GetImageQuantumDepth method is:
%
%      size_t GetImageQuantumDepth(const Image *image,
%        const MagickBooleanType constrain)
%
%  A description of each parameter follows:
%
%    o image: the image.
%
%    o constrain: A value other than MagickFalse, constrains the depth to
%      a maximum of MAGICKCORE_QUANTUM_DEPTH.
%
*/
MagickExport size_t GetImageQuantumDepth(const Image *image,
  const MagickBooleanType constrain)
{
  size_t
    depth;

  depth=image->depth;
  if (depth <= 8)
    depth=8;
  else
    if (depth <= 16)
      depth=16;
    else
      if (depth <= 32)
        depth=32;
      else
        if (depth <= 64)
          depth=64;
  if (constrain != MagickFalse)
    depth=(size_t) MagickMin((double) depth,(double) MAGICKCORE_QUANTUM_DEPTH);
  return(depth);
}

/*
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
%                                                                             %
%                                                                             %
%                                                                             %
%   G e t I m a g e T y p e                                                   %
%                                                                             %
%                                                                             %
%                                                                             %
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
%
%  GetImageType() returns the potential type of image:
%
%        Bilevel         Grayscale        GrayscaleMatte
%        Palette         PaletteMatte     TrueColor
%        TrueColorMatte  ColorSeparation  ColorSeparationMatte
%
%  To ensure the image type matches its potential, use SetImageType():
%
%    (void) SetImageType(image,GetImageType(image));
%
%  The format of the GetImageType method is:
%
%      ImageType GetImageType(const Image *image,ExceptionInfo *exception)
%
%  A description of each parameter follows:
%
%    o image: the image.
%
%    o exception: return any errors or warnings in this structure.
%
*/
MagickExport ImageType GetImageType(const Image *image,ExceptionInfo *exception)
{
  assert(image != (Image *) NULL);
  assert(image->signature == MagickCoreSignature);
  if (IsEventLogging() != MagickFalse)
    (void) LogMagickEvent(TraceEvent,GetMagickModule(),"%s",image->filename);
  if (image->colorspace == CMYKColorspace)
    {
      if (image->matte == MagickFalse)
        return(ColorSeparationType);
      return(ColorSeparationMatteType);
    }
  if (IsMonochromeImage(image,exception) != MagickFalse)
    return(BilevelType);
  if (IsGrayImage(image,exception) != MagickFalse)
    {
      if (image->matte != MagickFalse)
        return(GrayscaleMatteType);
      return(GrayscaleType);
    }
  if (IsPaletteImage(image,exception) != MagickFalse)
    {
      if (image->matte != MagickFalse)
        return(PaletteMatteType);
      return(PaletteType);
    }
  if (image->matte != MagickFalse)
    return(TrueColorMatteType);
  return(TrueColorType);
}

/*
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
%                                                                             %
%                                                                             %
%                                                                             %
%     I d e n t i f y I m a g e G r a y                                       %
%                                                                             %
%                                                                             %
%                                                                             %
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
%
%  either 0 or QuantumRange. Otherwise undefined is returned.
%
%  The format of the IdentifyImageGray method is:
%
%      ImageType IdentifyImageGray(const Image *image,ExceptionInfo *exception)
%
%  A description of each parameter follows:
%
%    o image: the image.
%
%    o exception: return any errors or warnings in this structure.
%
*/
MagickExport ImageType IdentifyImageGray(const Image *image,
  ExceptionInfo *exception)
{
  CacheView
    *image_view;

  ImageType
    type = BilevelType;

  ssize_t
    y;

  assert(image != (Image *) NULL);
  assert(image->signature == MagickCoreSignature);
  if (IsEventLogging() != MagickFalse)
    (void) LogMagickEvent(TraceEvent,GetMagickModule(),"%s",image->filename);
  if ((image->type == BilevelType) || (image->type == GrayscaleType) ||
      (image->type == GrayscaleMatteType))
    return(image->type);
  if (IssRGBCompatibleColorspace(image->colorspace) == MagickFalse)
    return(UndefinedType);
  image_view=AcquireVirtualCacheView(image,exception);
#if defined(MAGICKCORE_OPENMP_SUPPORT)
  #pragma omp parallel for schedule(static) shared(type) \
    magick_number_threads(image,image,image->rows,2)
#endif
  for (y=0; y < (ssize_t) image->rows; y++)
  {
    const PixelPacket
      *p;

    ssize_t
      x;

    if (type == UndefinedType)
      continue;
    p=GetCacheViewVirtualPixels(image_view,0,y,image->columns,1,exception);
    if (p == (const PixelPacket *) NULL)
      {
        type=UndefinedType;
        continue;
      }
    for (x=0; x < (ssize_t) image->columns; x++)
    {
      if (IsPixelGray(p) == MagickFalse)
        {
          type=UndefinedType;
          break;
        }
      if ((type == BilevelType) && (IsPixelMonochrome(p) == MagickFalse))
        type=GrayscaleType;
      p++;
    }
  }
  image_view=DestroyCacheView(image_view);
  if ((type == GrayscaleType) && (image->matte != MagickFalse))
    type=GrayscaleMatteType;
  return(type);
}

/*
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
%                                                                             %
%                                                                             %
%                                                                             %
%   I d e n t i f y I m a g e M o n o c h r o m e                             %
%                                                                             %
%                                                                             %
%                                                                             %
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
%
%  IdentifyImageMonochrome() returns MagickTrue if all the pixels in the image
%  have the same red, green, and blue intensities and the intensity is either
%  0 or QuantumRange.
%
%  The format of the IdentifyImageMonochrome method is:
%
%      MagickBooleanType IdentifyImageMonochrome(const Image *image,
%        ExceptionInfo *exception)
%
%  A description of each parameter follows:
%
%    o image: the image.
%
%    o exception: return any errors or warnings in this structure.
%
*/
MagickExport MagickBooleanType IdentifyImageMonochrome(const Image *image,
  ExceptionInfo *exception)
{
  CacheView
    *image_view;

  ImageType
    type = BilevelType;

  ssize_t
    y;

  assert(image != (Image *) NULL);
  assert(image->signature == MagickCoreSignature);
  if (IsEventLogging() != MagickFalse)
    (void) LogMagickEvent(TraceEvent,GetMagickModule(),"%s",image->filename);
  if (image->type == BilevelType)
    return(MagickTrue);
  if (IssRGBCompatibleColorspace(image->colorspace) == MagickFalse)
    return(MagickFalse);
  image_view=AcquireVirtualCacheView(image,exception);
#if defined(MAGICKCORE_OPENMP_SUPPORT)
  #pragma omp parallel for schedule(static) shared(type) \
    magick_number_threads(image,image,image->rows,2)
#endif
  for (y=0; y < (ssize_t) image->rows; y++)
  {
    const PixelPacket
      *p;

    ssize_t
      x;

    if (type == UndefinedType)
      continue;
    p=GetCacheViewVirtualPixels(image_view,0,y,image->columns,1,exception);
    if (p == (const PixelPacket *) NULL)
      {
        type=UndefinedType;
        continue;
      }
    for (x=0; x < (ssize_t) image->columns; x++)
    {
      if (IsPixelMonochrome(p) == MagickFalse)
        {
          type=UndefinedType;
          break;
        }
      p++;
    }
  }
  image_view=DestroyCacheView(image_view);
  return(type == BilevelType ? MagickTrue : MagickFalse);
}

/*
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
%                                                                             %
%                                                                             %
%                                                                             %
%   I d e n t i f y I m a g e T y p e                                         %
%                                                                             %
%                                                                             %
%                                                                             %
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
%
%  IdentifyImageType() returns the potential type of image:
%
%        Bilevel         Grayscale        GrayscaleMatte
%        Palette         PaletteMatte     TrueColor
%        TrueColorMatte  ColorSeparation  ColorSeparationMatte
%
%  To ensure the image type matches its potential, use SetImageType():
%
%    (void) SetImageType(image,IdentifyImageType(image,exception),exception);
%
%  The format of the IdentifyImageType method is:
%
%      ImageType IdentifyImageType(const Image *image,ExceptionInfo *exception)
%
%  A description of each parameter follows:
%
%    o image: the image.
%
%    o exception: return any errors or warnings in this structure.
%
*/
MagickExport ImageType IdentifyImageType(const Image *image,
  ExceptionInfo *exception)
{
  assert(image != (Image *) NULL);
  assert(image->signature == MagickCoreSignature);
  if (IsEventLogging() != MagickFalse)
    (void) LogMagickEvent(TraceEvent,GetMagickModule(),"%s",image->filename);
  if (image->colorspace == CMYKColorspace)
    {
      if (image->matte == MagickFalse)
        return(ColorSeparationType);
      return(ColorSeparationMatteType);
    }
  if (IdentifyImageMonochrome(image,exception) != MagickFalse)
    return(BilevelType);
  if (IdentifyImageGray(image,exception) != UndefinedType)
    {
      if (image->matte != MagickFalse)
        return(GrayscaleMatteType);
      return(GrayscaleType);
    }
  if (IdentifyPaletteImage(image,exception) != MagickFalse)
    {
      if (image->matte != MagickFalse)
        return(PaletteMatteType);
      return(PaletteType);
    }
  if (image->matte != MagickFalse)
    return(TrueColorMatteType);
  return(TrueColorType);
}

/*
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
%                                                                             %
%                                                                             %
%                                                                             %
%     I s G r a y I m a g e                                                   %
%                                                                             %
%                                                                             %
%                                                                             %
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
%
%  IsGrayImage() returns MagickTrue if the type of the image is grayscale or
%  bi-level.
%
%  The format of the IsGrayImage method is:
%
%      MagickBooleanType IsGrayImage(const Image *image,
%        ExceptionInfo *exception)
%
%  A description of each parameter follows:
%
%    o image: the image.
%
%    o exception: return any errors or warnings in this structure.
%
*/
MagickExport MagickBooleanType IsGrayImage(const Image *image,
  ExceptionInfo *magick_unused(exception))
{
  assert(image != (Image *) NULL);
  assert(image->signature == MagickCoreSignature);
  magick_unreferenced(exception);
  if ((image->type == BilevelType) || (image->type == GrayscaleType) ||
      (image->type == GrayscaleMatteType))
    return(MagickTrue);
  return(MagickFalse);
}

/*
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
%                                                                             %
%                                                                             %
%                                                                             %
%   I s M o n o c h r o m e I m a g e                                         %
%                                                                             %
%                                                                             %
%                                                                             %
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
%
%  IsMonochromeImage() returns MagickTrue if type of the image is bi-level.
%
%  The format of the IsMonochromeImage method is:
%
%      MagickBooleanType IsMonochromeImage(const Image *image,
%        ExceptionInfo *exception)
%
%  A description of each parameter follows:
%
%    o image: the image.
%
%    o exception: return any errors or warnings in this structure.
%
*/
MagickExport MagickBooleanType IsMonochromeImage(const Image *image,
  ExceptionInfo *magick_unused(exception))
{
  assert(image != (Image *) NULL);
  assert(image->signature == MagickCoreSignature);
  magick_unreferenced(exception);
  if (image->type == BilevelType)
    return(MagickTrue);
  return(MagickFalse);
}

/*
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
%                                                                             %
%                                                                             %
%                                                                             %
%     I s O p a q u e I m a g e                                               %
%                                                                             %
%                                                                             %
%                                                                             %
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
%
%  IsOpaqueImage() returns MagickTrue if none of the pixels in the image have
%  an opacity value other than opaque (0).
%
%  The format of the IsOpaqueImage method is:
%
%      MagickBooleanType IsOpaqueImage(const Image *image,
%        ExceptionInfo *exception)
%
%  A description of each parameter follows:
%
%    o image: the image.
%
%    o exception: return any errors or warnings in this structure.
%
*/
MagickExport MagickBooleanType IsOpaqueImage(const Image *image,
  ExceptionInfo *exception)
{
  CacheView
    *image_view;

  MagickBooleanType
    opaque = MagickTrue;

  ssize_t
    y;

  /*
    Determine if image is opaque.
  */
  assert(image != (Image *) NULL);
  assert(image->signature == MagickCoreSignature);
  if (IsEventLogging() != MagickFalse)
    (void) LogMagickEvent(TraceEvent,GetMagickModule(),"%s",image->filename);
  if (image->matte == MagickFalse)
    return(MagickTrue);
  image_view=AcquireVirtualCacheView(image,exception);
#if defined(MAGICKCORE_OPENMP_SUPPORT)
  #pragma omp parallel for schedule(static) shared(opaque) \
    magick_number_threads(image,image,image->rows,2)
#endif
  for (y=0; y < (ssize_t) image->rows; y++)
  {
    const PixelPacket
      *p;

    ssize_t
      x;

    if (opaque == MagickFalse)
      continue;
    p=GetCacheViewVirtualPixels(image_view,0,y,image->columns,1,exception);
    if (p == (const PixelPacket *) NULL)
      {
        opaque=MagickFalse;
        continue;
      }
    for (x=0; x < (ssize_t) image->columns; x++)
    {
      if (GetPixelOpacity(p) != OpaqueOpacity)
        {
          opaque=MagickFalse;
          break;
        }
      p++;
    }
  }
  image_view=DestroyCacheView(image_view);
  return(opaque);
}

/*
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
%                                                                             %
%                                                                             %
%                                                                             %
%   S e t I m a g e C h a n n e l D e p t h                                   %
%                                                                             %
%                                                                             %
%                                                                             %
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
%
%  SetImageChannelDepth() sets the depth of the image.
%
%  The format of the SetImageChannelDepth method is:
%
%      MagickBooleanType SetImageDepth(Image *image,const size_t depth)
%      MagickBooleanType SetImageChannelDepth(Image *image,
%        const ChannelType channel,const size_t depth)
%
%  A description of each parameter follows:
%
%    o image: the image.
%
%    o channel: the channel.
%
%    o depth: the image depth.
%
*/

MagickExport MagickBooleanType SetImageDepth(Image *image,
  const size_t depth)
{
  return(SetImageChannelDepth(image,CompositeChannels,depth));
}

MagickExport MagickBooleanType SetImageChannelDepth(Image *image,
  const ChannelType channel,const size_t depth)
{
  CacheView
    *image_view;

  ExceptionInfo
    *exception;

  MagickBooleanType
    status;

  QuantumAny
    range;

  ssize_t
    y;

  assert(image != (Image *) NULL);
  if (IsEventLogging() != MagickFalse)
    (void) LogMagickEvent(TraceEvent,GetMagickModule(),"...");
  assert(image->signature == MagickCoreSignature);
  if (depth >= MAGICKCORE_QUANTUM_DEPTH)
    {
      image->depth=depth;
      return(MagickTrue);
    }
  range=GetQuantumRange(depth);
  if (image->storage_class == PseudoClass)
    {
      ssize_t
        i;

#if defined(MAGICKCORE_OPENMP_SUPPORT)
      #pragma omp parallel for schedule(static) shared(status) \
        magick_number_threads(image,image,image->rows,1)
#endif
      for (i=0; i < (ssize_t) image->colors; i++)
      {
        if ((channel & RedChannel) != 0)
          image->colormap[i].red=ScaleAnyToQuantum(ScaleQuantumToAny(
            ClampPixel((MagickRealType) image->colormap[i].red),range),range);
        if ((channel & GreenChannel) != 0)
          image->colormap[i].green=ScaleAnyToQuantum(ScaleQuantumToAny(
            ClampPixel((MagickRealType) image->colormap[i].green),range),range);
        if ((channel & BlueChannel) != 0)
          image->colormap[i].blue=ScaleAnyToQuantum(ScaleQuantumToAny(
            ClampPixel((MagickRealType) image->colormap[i].blue),range),range);
        if ((channel & OpacityChannel) != 0)
          image->colormap[i].opacity=ScaleAnyToQuantum(ScaleQuantumToAny(
            ClampPixel((MagickRealType) image->colormap[i].opacity),range),
              range);
      }
    }
  status=MagickTrue;
  exception=(&image->exception);
  image_view=AcquireAuthenticCacheView(image,exception);
#if !defined(MAGICKCORE_HDRI_SUPPORT)
DisableMSCWarning(4127)
  if (1UL*QuantumRange <= MaxMap)
RestoreMSCWarning
    {
      Quantum
        *depth_map;

      ssize_t
        i;

      /*
        Scale pixels to desired (optimized with depth map).
      */
      depth_map=(Quantum *) AcquireQuantumMemory(MaxMap+1,sizeof(*depth_map));
      if (depth_map == (Quantum *) NULL)
        ThrowFatalException(ResourceLimitFatalError,"MemoryAllocationFailed");
      for (i=0; i <= (ssize_t) MaxMap; i++)
        depth_map[i]=ScaleAnyToQuantum(ScaleQuantumToAny((Quantum) i,range),
          range);
#if defined(MAGICKCORE_OPENMP_SUPPORT)
      #pragma omp parallel for schedule(static) shared(status) \
        magick_number_threads(image,image,image->rows,2)
#endif
      for (y=0; y < (ssize_t) image->rows; y++)
      {
        PixelPacket
          *magick_restrict q;

        ssize_t
          x;

        if (status == MagickFalse)
          continue;
        q=GetCacheViewAuthenticPixels(image_view,0,y,image->columns,1,
          exception);
        if (q == (PixelPacket *) NULL)
          {
            status=MagickFalse;
            continue;
          }
        for (x=0; x < (ssize_t) image->columns; x++)
        {
          if ((channel & RedChannel) != 0)
            SetPixelRed(q,depth_map[ScaleQuantumToMap(GetPixelRed(q))]);
          if ((channel & GreenChannel) != 0)
            SetPixelGreen(q,depth_map[ScaleQuantumToMap(GetPixelGreen(q))]);
          if ((channel & BlueChannel) != 0)
            SetPixelBlue(q,depth_map[ScaleQuantumToMap(GetPixelBlue(q))]);
          if (((channel & OpacityChannel) != 0) &&
              (image->matte != MagickFalse))
            SetPixelOpacity(q,depth_map[ScaleQuantumToMap(GetPixelOpacity(q))]);
          q++;
        }
        if (SyncCacheViewAuthenticPixels(image_view,exception) == MagickFalse)
          {
            status=MagickFalse;
            continue;
          }
      }
      image_view=DestroyCacheView(image_view);
      depth_map=(Quantum *) RelinquishMagickMemory(depth_map);
      if (status != MagickFalse)
        image->depth=depth;
      return(status);
    }
#endif
  /*
    Scale pixels to desired depth.
  */
#if defined(MAGICKCORE_OPENMP_SUPPORT)
  #pragma omp parallel for schedule(static) shared(status) \
    magick_number_threads(image,image,image->rows,2)
#endif
  for (y=0; y < (ssize_t) image->rows; y++)
  {
    PixelPacket
      *magick_restrict q;

    ssize_t
      x;

    if (status == MagickFalse)
      continue;
    q=GetCacheViewAuthenticPixels(image_view,0,y,image->columns,1,exception);
    if (q == (PixelPacket *) NULL)
      {
        status=MagickFalse;
        continue;
      }
    for (x=0; x < (ssize_t) image->columns; x++)
    {
      if ((channel & RedChannel) != 0)
        SetPixelRed(q,ScaleAnyToQuantum(ScaleQuantumToAny(ClampPixel(
          (MagickRealType) GetPixelRed(q)),range),range));
      if ((channel & GreenChannel) != 0)
        SetPixelGreen(q,ScaleAnyToQuantum(ScaleQuantumToAny(ClampPixel(
          (MagickRealType) GetPixelGreen(q)),range),range));
      if ((channel & BlueChannel) != 0)
        SetPixelBlue(q,ScaleAnyToQuantum(ScaleQuantumToAny(ClampPixel(
          (MagickRealType) GetPixelBlue(q)),range),range));
      if (((channel & OpacityChannel) != 0) && (image->matte != MagickFalse))
        SetPixelOpacity(q,ScaleAnyToQuantum(ScaleQuantumToAny(ClampPixel(
          (MagickRealType) GetPixelOpacity(q)),range),range));
      q++;
    }
    if (SyncCacheViewAuthenticPixels(image_view,exception) == MagickFalse)
      {
        status=MagickFalse;
        continue;
      }
  }
  image_view=DestroyCacheView(image_view);
  if (status != MagickFalse)
    image->depth=depth;
  return(status);
}

/*
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
%                                                                             %
%                                                                             %
%                                                                             %
%   S e t I m a g e T y p e                                                   %
%                                                                             %
%                                                                             %
%                                                                             %
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
%
%  SetImageType() sets the type of image.  Choose from these types:
%
%      BilevelType, GrayscaleType, GrayscaleMatteType, PaletteType,
%      PaletteMatteType, TrueColorType, TrueColorMatteType,
%      ColorSeparationType, ColorSeparationMatteType, OptimizeType
%
%  The format of the SetImageType method is:
%
%      MagickBooleanType SetImageType(Image *image,const ImageType type)
%
%  A description of each parameter follows:
%
%    o image: the image.
%
%    o type: Image type.
%
*/
MagickExport MagickBooleanType SetImageType(Image *image,const ImageType type)
{
  const char
    *artifact;

  ImageInfo
    *image_info;

  MagickBooleanType
    status;

  QuantizeInfo
    *quantize_info;

  assert(image != (Image *) NULL);
  if (IsEventLogging() != MagickFalse)
    (void) LogMagickEvent(TraceEvent,GetMagickModule(),"...");
  assert(image->signature == MagickCoreSignature);
  status=MagickTrue;
  image_info=AcquireImageInfo();
  image_info->dither=image->dither;
  artifact=GetImageArtifact(image,"dither");
  if (artifact != (const char *) NULL)
    (void) SetImageOption(image_info,"dither",artifact);
  switch (type)
  {
    case BilevelType:
    {
      if (IsGrayColorspace(image->colorspace) == MagickFalse)
        status=TransformImageColorspace(image,GRAYColorspace);
      (void) NormalizeImage(image);
      (void) BilevelImage(image,(double) QuantumRange/2.0);
      quantize_info=AcquireQuantizeInfo(image_info);
      quantize_info->number_colors=2;
      quantize_info->colorspace=GRAYColorspace;
      status=QuantizeImage(quantize_info,image);
      quantize_info=DestroyQuantizeInfo(quantize_info);
      image->matte=MagickFalse;
      break;
    }
    case GrayscaleType:
    {
      if (IsGrayColorspace(image->colorspace) == MagickFalse)
        status=TransformImageColorspace(image,GRAYColorspace);
      image->matte=MagickFalse;
      break;
    }
    case GrayscaleMatteType:
    {
      if (IsGrayColorspace(image->colorspace) == MagickFalse)
        status=TransformImageColorspace(image,GRAYColorspace);
      if (image->matte == MagickFalse)
        (void) SetImageAlphaChannel(image,OpaqueAlphaChannel);
      break;
    }
    case PaletteType:
    {
      if (IssRGBCompatibleColorspace(image->colorspace) == MagickFalse)
        status=TransformImageColorspace(image,sRGBColorspace);
      if ((image->storage_class == DirectClass) || (image->colors > 256))
        {
          quantize_info=AcquireQuantizeInfo(image_info);
          quantize_info->number_colors=256;
          status=QuantizeImage(quantize_info,image);
          quantize_info=DestroyQuantizeInfo(quantize_info);
        }
      image->matte=MagickFalse;
      break;
    }
    case PaletteBilevelMatteType:
    {
      if (IssRGBCompatibleColorspace(image->colorspace) == MagickFalse)
        status=TransformImageColorspace(image,sRGBColorspace);
      if (image->matte == MagickFalse)
        (void) SetImageAlphaChannel(image,OpaqueAlphaChannel);
      (void) BilevelImageChannel(image,AlphaChannel,(double) QuantumRange/2.0);
      quantize_info=AcquireQuantizeInfo(image_info);
      status=QuantizeImage(quantize_info,image);
      quantize_info=DestroyQuantizeInfo(quantize_info);
      break;
    }
    case PaletteMatteType:
    {
      if (IssRGBCompatibleColorspace(image->colorspace) == MagickFalse)
        status=TransformImageColorspace(image,sRGBColorspace);
      if (image->matte == MagickFalse)
        (void) SetImageAlphaChannel(image,OpaqueAlphaChannel);
      quantize_info=AcquireQuantizeInfo(image_info);
      quantize_info->colorspace=TransparentColorspace;
      status=QuantizeImage(quantize_info,image);
      quantize_info=DestroyQuantizeInfo(quantize_info);
      break;
    }
    case TrueColorType:
    {
      if (IssRGBCompatibleColorspace(image->colorspace) == MagickFalse)
        status=TransformImageColorspace(image,sRGBColorspace);
      if (image->storage_class != DirectClass)
        status=SetImageStorageClass(image,DirectClass);
      image->matte=MagickFalse;
      break;
    }
    case TrueColorMatteType:
    {
      if (IssRGBCompatibleColorspace(image->colorspace) == MagickFalse)
        status=TransformImageColorspace(image,sRGBColorspace);
      if (image->storage_class != DirectClass)
        status=SetImageStorageClass(image,DirectClass);
      if (image->matte == MagickFalse)
        (void) SetImageAlphaChannel(image,OpaqueAlphaChannel);
      break;
    }
    case ColorSeparationType:
    {
      if (image->colorspace != CMYKColorspace)
        status=TransformImageColorspace(image,CMYKColorspace);
      if (image->storage_class != DirectClass)
        status=SetImageStorageClass(image,DirectClass);
      image->matte=MagickFalse;
      break;
    }
    case ColorSeparationMatteType:
    {
      if (image->colorspace != CMYKColorspace)
        status=TransformImageColorspace(image,CMYKColorspace);
      if (image->storage_class != DirectClass)
        status=SetImageStorageClass(image,DirectClass);
      if (image->matte == MagickFalse)
        (void) SetImageAlphaChannel(image,OpaqueAlphaChannel);
      break;
    }
    case OptimizeType:
    case UndefinedType:
      break;
  }
  image_info=DestroyImageInfo(image_info);
  if (status == MagickFalse)
    return(MagickFalse);
  image->type=type;
  return(MagickTrue);
}
