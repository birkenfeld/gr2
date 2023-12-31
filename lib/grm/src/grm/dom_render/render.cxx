#ifdef _WIN32
/*
 * Headers on Windows can define `min` and `max` as macros which causes
 * problem when using `std::min` and `std::max`
 * -> Define `NOMINMAX` to prevent the definition of these macros
 */
#define NOMINMAX
#endif

#define _USE_MATH_DEFINES


#include <functional>
#include <vector>
#include <array>
#include <set>
#include <sstream>
#include <algorithm>
#include <numeric>
#include <cmath>
#include <cfloat>
#include <climits>
#include <grm/dom_render/graphics_tree/Element.hxx>
#include <grm/dom_render/graphics_tree/Document.hxx>
#include <grm/dom_render/graphics_tree/Value.hxx>
#include <grm/dom_render/graphics_tree/util.hxx>
#include <grm/dom_render/render.hxx>
#include <grm/dom_render/NotFoundError.hxx>
#include <grm/dom_render/context.hxx>
#include "gks.h"
#include "gr.h"
#include "gr3.h"
#include "grm/layout.hxx"
#include "grm/plot_int.h"
#include <cm.h>
#include "grm/utilcpp_int.hxx"
#include "grm/dom_render/ManageZIndex.hxx"
#include "grm/dom_render/Drawable.hxx"
#include "grm/dom_render/ManageGRContextIds.hxx"
#include "grm/dom_render/ManageCustomColorIndex.hxx"
extern "C" {
#include "grm/datatype/string_map_int.h"
}

/* ------------------------- re-implementation of x_lin/x_log ------------------------------------------------------- */

#define X_FLIP_IF(x, scale_options, xmin, xmax) \
  ((GR_OPTION_FLIP_X & (scale_options) ? (xmin) + (xmax) : 0) + (GR_OPTION_FLIP_X & (scale_options) ? -1 : 1) * (x))

#define X_LIN(x, scale_options, xmin, xmax, a, b)                                                                 \
  X_FLIP_IF((GR_OPTION_X_LOG & (scale_options) ? ((x) > 0 ? (a)*log10(x) + (b) : -FLT_MAX) : (x)), scale_options, \
            xmin, xmax)

#define X_LOG(x, scale_options, xmin, xmax, a, b)                                                                   \
  (GR_OPTION_X_LOG & (scale_options) ? (pow(10.0, (double)((X_FLIP_IF(x, scale_options, xmin, xmax) - (b)) / (a)))) \
                                     : X_FLIP_IF(x, scale_options, xmin, xmax))

std::shared_ptr<GRM::Element> global_root;
std::shared_ptr<GRM::Element> active_figure;
std::shared_ptr<GRM::Render> global_render;
std::priority_queue<std::shared_ptr<Drawable>, std::vector<std::shared_ptr<Drawable>>, CompareZIndex> z_queue;
bool zQueueIsBeingRendered = false;
std::map<std::shared_ptr<GRM::Element>, int> parent_to_context;
ManageGRContextIds grContextIDManager;
ManageZIndex zIndexManager;
ManageCustomColorIndex customColorIndexManager;

//! This vector is used for storing element types which children get processed. Other types' children will be ignored
static std::set<std::string> parentTypes = {
    "axes",
    "axes_text_group",
    "bar",
    "colorbar",
    "coordinate_system",
    "errorbar",
    "errorbars",
    "figure",
    "label",
    "labels_group",
    "layout_grid",
    "layout_gridelement",
    "legend",
    "pie_segment",
    "plot",
    "polar_axes",
    "polar_bar",
    "root",
    "series_barplot",
    "series_contour",
    "series_contourf",
    "series_heatmap",
    "series_hexbin",
    "series_hist",
    "series_imshow",
    "series_isosurface",
    "series_line",
    "series_marginalheatmap",
    "series_nonuniformheatmap",
    "series_nonuniformpolar_heatmap",
    "series_pie",
    "series_plot3",
    "series_polar",
    "series_polar_heatmap",
    "series_polar_histogram",
    "series_quiver",
    "series_scatter",
    "series_scatter3",
    "series_shade",
    "series_stairs",
    "series_stem",
    "series_surface",
    "series_tricontour",
    "series_trisurface",
    "series_volume",
    "series_wireframe",
    "xticklabel_group",
    "yticklabel_group",
};

static std::set<std::string> drawableTypes = {
    "axes",
    "axes3d",
    "cellarray",
    "drawarc",
    "drawgraphics",
    "drawimage",
    "drawrect",
    "fillarc",
    "fillarea",
    "fillrect",
    "gr3clear",
    "gr3deletemesh",
    "gr3drawimage",
    "gr3drawmesh",
    "grid",
    "grid3d",
    "isosurface_render",
    "layout_grid",
    "layout_gridelement",
    "legend",
    "nonuniformcellarray",
    "panzoom",
    "polyline",
    "polyline3d",
    "polymarker",
    "polymarker3d",
    "text",
    "titles3d",
};

static std::set<std::string> drawableKinds = {
    "contour", "contourf", "hexbin", "isosurface", "quiver", "shade", "surface", "tricontour", "trisurface", "volume",
};

static std::set<std::string> valid_context_keys = {"absolute_downwards",
                                                   "absolute_upwards",
                                                   "bar_color_rgb",
                                                   "bin_counts",
                                                   "bin_edges",
                                                   "bin_widths",
                                                   "bins",
                                                   "c",
                                                   "c_dims",
                                                   "c_rgb",
                                                   "classes",
                                                   "color",
                                                   "colors",
                                                   "color_indices",
                                                   "color_rgb_values",
                                                   "data",
                                                   "directions",
                                                   "edge_color_rgb",
                                                   "foreground_color",
                                                   "img_data_key",
                                                   "indices",
                                                   "labels",
                                                   "linecolorinds",
                                                   "linetypes",
                                                   "linewidths",
                                                   "markercolorinds",
                                                   "markersizes",
                                                   "markertypes",
                                                   "phi",
                                                   "positions",
                                                   "px",
                                                   "py",
                                                   "pz",
                                                   "r",
                                                   "relative_downwards",
                                                   "relative_upwards",
                                                   "scales",
                                                   "specs",
                                                   "theta",
                                                   "u",
                                                   "ups",
                                                   "v",
                                                   "weights",
                                                   "x",
                                                   "xi",
                                                   "xticklabels",
                                                   "y",
                                                   "ylabels",
                                                   "yticklabels",
                                                   "z"};

static std::map<std::string, double> symbol_to_meters_per_unit{
    {"m", 1.0},     {"dm", 0.1},    {"cm", 0.01},  {"mm", 0.001},        {"in", 0.0254},
    {"\"", 0.0254}, {"ft", 0.3048}, {"'", 0.0254}, {"pc", 0.0254 / 6.0}, {"pt", 0.0254 / 72.0},
};

static int plot_scatter_markertypes[] = {
    GKS_K_MARKERTYPE_SOLID_CIRCLE,   GKS_K_MARKERTYPE_SOLID_TRI_UP, GKS_K_MARKERTYPE_SOLID_TRI_DOWN,
    GKS_K_MARKERTYPE_SOLID_SQUARE,   GKS_K_MARKERTYPE_SOLID_BOWTIE, GKS_K_MARKERTYPE_SOLID_HGLASS,
    GKS_K_MARKERTYPE_SOLID_DIAMOND,  GKS_K_MARKERTYPE_SOLID_STAR,   GKS_K_MARKERTYPE_SOLID_TRI_RIGHT,
    GKS_K_MARKERTYPE_SOLID_TRI_LEFT, GKS_K_MARKERTYPE_SOLID_PLUS,   GKS_K_MARKERTYPE_PENTAGON,
    GKS_K_MARKERTYPE_HEXAGON,        GKS_K_MARKERTYPE_HEPTAGON,     GKS_K_MARKERTYPE_OCTAGON,
    GKS_K_MARKERTYPE_STAR_4,         GKS_K_MARKERTYPE_STAR_5,       GKS_K_MARKERTYPE_STAR_6,
    GKS_K_MARKERTYPE_STAR_7,         GKS_K_MARKERTYPE_STAR_8,       GKS_K_MARKERTYPE_VLINE,
    GKS_K_MARKERTYPE_HLINE,          GKS_K_MARKERTYPE_OMARK,        INT_MAX};
static int *previous_scatter_marker_type = plot_scatter_markertypes;
static int *previous_line_marker_type = plot_scatter_markertypes;

static int bounding_id = 0;
static bool automatic_update = false;
static bool redrawws = false;
static std::map<int, std::shared_ptr<GRM::Element>> bounding_map;

static string_map_entry_t kind_to_fmt[] = {{"line", "xys"},           {"hexbin", "xys"},
                                           {"polar", "xys"},          {"shade", "xys"},
                                           {"stem", "xys"},           {"stairs", "xys"},
                                           {"contour", "xyzc"},       {"contourf", "xyzc"},
                                           {"tricont", "xyzc"},       {"trisurf", "xyzc"},
                                           {"surface", "xyzc"},       {"wireframe", "xyzc"},
                                           {"plot3", "xyzc"},         {"scatter", "xyzc"},
                                           {"scatter3", "xyzc"},      {"quiver", "xyuv"},
                                           {"heatmap", "xyzc"},       {"hist", "x"},
                                           {"barplot", "y"},          {"isosurface", "c"},
                                           {"imshow", "c"},           {"nonuniformheatmap", "xyzc"},
                                           {"polar_histogram", "x"},  {"pie", "x"},
                                           {"volume", "c"},           {"marginalheatmap", "xyzc"},
                                           {"polar_heatmap", "xyzc"}, {"nonuniformpolar_heatmap", "xyzc"}};

static string_map_t *fmt_map = string_map_new_with_data(array_size(kind_to_fmt), kind_to_fmt);

enum class del_values
{
  update_without_default = 0,
  update_with_default = 1,
  recreate_own_children = 2,
  recreate_all_children = 3
};

/* ~~~~~~~~~~~~~~~~~~~~~~~~~ utility functions ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

static double getLightness(int color)
{
  unsigned char rgb[sizeof(int)];

  gr_inqcolor(color, (int *)rgb);
  double Y = (0.2126729 * rgb[0] / 255 + 0.7151522 * rgb[1] / 255 + 0.0721750 * rgb[2] / 255);
  return 116 * pow(Y / 100, 1.0 / 3) - 16;
}

static void resetOldBoundingBoxes(const std::shared_ptr<GRM::Element> &element)
{
  element->setAttribute("_bbox_id", -1);
  element->removeAttribute("_bbox_xmin");
  element->removeAttribute("_bbox_xmax");
  element->removeAttribute("_bbox_ymin");
  element->removeAttribute("_bbox_ymax");
}

static void clearOldChildren(del_values *del, const std::shared_ptr<GRM::Element> &element)
{
  /* clear all old children of an element when del is 2 or 3, in the special case where no children exist del gets
   * manipulated so that new children will be created in caller functions*/
  if (*del != del_values::update_without_default && *del != del_values::update_with_default)
    {
      for (const auto &child : element->children())
        {
          if (*del == del_values::recreate_own_children)
            {
              if (child->hasAttribute("_child_id")) child->remove();
            }
          else if (*del == del_values::recreate_all_children)
            {
              child->remove();
            }
        }
    }
  else if (!element->hasChildNodes())
    *del = del_values::recreate_own_children;
  else
    {
      bool only_errorchild = true;
      for (const auto &child : element->children())
        {
          if (child->localName() != "errorbars")
            {
              only_errorchild = false;
              break;
            }
        }
      if (only_errorchild) *del = del_values::recreate_own_children;
    }
}

static std::string getLocalName(const std::shared_ptr<GRM::Element> &element)
{
  std::string local_name = element->localName();
  if (starts_with(element->localName(), "series")) local_name = "series";
  return local_name;
}

static bool isDrawable(const std::shared_ptr<GRM::Element> &element)
{
  auto local_name = getLocalName(element);
  if (drawableTypes.find(local_name) != drawableTypes.end())
    {
      return true;
    }
  if (local_name == "series")
    {
      auto kind = static_cast<std::string>(element->getAttribute("kind"));
      if (drawableKinds.find(kind) != drawableKinds.end())
        {
          return true;
        }
    }
  return false;
}

int getVolumeAlgorithm(const std::shared_ptr<GRM::Element> &element)
{
  int algorithm;
  std::string algorithm_str;

  if (element->getAttribute("algorithm").isInt())
    {
      algorithm = static_cast<int>(element->getAttribute("algorithm"));
    }
  else if (element->getAttribute("algorithm").isString())
    {
      algorithm_str = static_cast<std::string>(element->getAttribute("algorithm"));
      if (algorithm_str == "emission")
        {
          algorithm = GR_VOLUME_EMISSION;
        }
      else if (algorithm_str == "absorption")
        {
          algorithm = GR_VOLUME_ABSORPTION;
        }
      else if (algorithm_str == "mip" || algorithm_str == "maximum")
        {
          algorithm = GR_VOLUME_MIP;
        }
      else
        {
          logger((stderr, "Got unknown volume algorithm \"%s\"\n", algorithm_str.c_str()));
          throw std::logic_error("For volume series the given algorithm is unknown.\n");
        }
    }
  else
    {
      throw NotFoundError("Volume series is missing attribute algorithm.\n");
    }
  return algorithm;
}

PushDrawableToZQueue::PushDrawableToZQueue(
    std::function<void(const std::shared_ptr<GRM::Element> &, const std::shared_ptr<GRM::Context> &)> drawFunction)
    : drawFunction(drawFunction)
{
  ;
}

void PushDrawableToZQueue::operator()(const std::shared_ptr<GRM::Element> element,
                                      const std::shared_ptr<GRM::Context> context)
{
  auto parent = element->parentElement();
  int contextID;
  if (auto search = parent_to_context.find(parent); search != parent_to_context.end())
    {
      contextID = search->second;
    }
  else
    {
      contextID = grContextIDManager.getUnusedGRContextId();
      gr_savecontext(contextID);
      parent_to_context[parent] = contextID;
    }
  auto drawable =
      std::shared_ptr<Drawable>(new Drawable(element, context, contextID, zIndexManager.getZIndex(), drawFunction));
  drawable->insertionIndex = z_queue.size();
  customColorIndexManager.savecontext(contextID);
  z_queue.push(drawable);
}

static void getPlotParent(std::shared_ptr<GRM::Element> &element)
{
  auto plot_parent = element;
  while (plot_parent->localName() != "plot")
    {
      plot_parent = plot_parent->parentElement();
    }
  element = plot_parent;
}

static double auto_tick(double amin, double amax)
{
  double tick_size[] = {5.0, 2.0, 1.0, 0.5, 0.2, 0.1, 0.05, 0.02, 0.01};
  double scale, tick;
  int i, n;

  scale = pow(10.0, (int)(log10(amax - amin)));
  tick = 1.0;
  for (i = 0; i < 9; i++)
    {
      n = (amax - amin) / scale / tick_size[i];
      if (n > 7)
        {
          tick = tick_size[i - 1];
          break;
        }
    }
  tick *= scale;
  return tick;
}

static double auto_tick_rings_polar(double rmax, int &rings, const std::string &norm)
{
  double scale;
  bool decimal = false;

  std::vector<int> largeRings = {6, 7, 8, 9, 10, 11, 12, 13, 14, 15};
  std::vector<int> normalRings = {3, 4, 5, 6, 7};

  std::vector<int> *whichVector;

  // -1 --> auto rings
  if (rings == -1)
    {
      if (norm == "cdf")
        {
          rings = 4;
          return 1.0 / rings;
        }

      if (rmax > 20)
        {
          whichVector = &largeRings;
        }
      else
        {
          whichVector = &normalRings;
        }
      scale = ceil(abs(log10(rmax)));
      if (rmax < 1.0)
        {
          decimal = true;
          rmax = static_cast<int>(ceil(rmax * pow(10.0, scale)));
        }

      while (true)
        {
          for (int i : *whichVector)
            {
              if (static_cast<int>(rmax) % i == 0)
                {
                  if (decimal)
                    {
                      rmax = rmax / pow(10.0, scale);
                    }
                  rings = i;
                  return rmax / rings;
                }
            }
          // rmax not divisible by whichVector
          ++rmax;
        }
    }

  // given rings
  if (norm == "cdf")
    {
      return 1.0 / rings;
    }

  if (rmax > rings)
    {
      return (static_cast<int>(rmax) + (rings - (static_cast<int>(rmax) % rings))) / rings;
    }
  else if (rmax > (rings * 0.6))
    {
      // returns rings / rings -> 1.0 so that rmax = rings * tick -> rings. Number of rings is rmax then
      return 1.0;
    }
  scale = ceil(abs(log10(rmax)));
  rmax = static_cast<int>(rmax * pow(10.0, scale));
  if (static_cast<int>(rmax) % rings == 0)
    {
      rmax = rmax / pow(10.0, scale);
      return rmax / rings;
    }
  rmax += rings - (static_cast<int>(rmax) % rings);
  rmax = rmax / pow(10.0, scale);

  return rmax / rings;
}

/*!
 * Convert an RGB triple to a luminance value following the CCIR 601 format.
 *
 * \param[in] r The red component of the RGB triple in the range [0.0, 1.0].
 * \param[in] g The green component of the RGB triple in the range [0.0, 1.0].
 * \param[in] b The blue component of the RGB triple in the range [0.0, 1.0].
 * \return The luminance of the given RGB triple in the range [0.0, 1.0].
 */
static double get_lightness_from_rbg(double r, double g, double b)
{
  return 0.299 * r + 0.587 * g + 0.114 * b;
}

/*
 * mixes gr colormaps with size = size * size. If x and or y < 0
 * */
void create_colormap(int x, int y, int size, std::vector<int> &colormap)
{
  int r, g, b, a;
  int outer, inner;
  int r1, g1, b1;
  int r2, g2, b2;
  if (x > 47 || y > 47)
    {
      logger((stderr, "values for the keyword \"colormap\" can not be greater than 47\n"));
    }

  colormap.resize(size * size);
  if (x >= 0 && y < 0)
    {
      for (outer = 0; outer < size; outer++)
        {
          for (inner = 0; inner < size; inner++)
            {
              a = 255;
              r = ((cmap_h[x][(int)(inner * 255.0 / size)] >> 16) & 0xff);
              g = ((cmap_h[x][(int)(inner * 255.0 / size)] >> 8) & 0xff);
              b = (cmap_h[x][(int)(inner * 255.0 / size)] & 0xff);

              colormap[outer * size + inner] = (a << 24) + (b << 16) + (g << 8) + (r);
            }
        }
    }

  if (x < 0 && y >= 0)
    {
      gr_setcolormap(y);
      for (outer = 0; outer < size; outer++)
        {
          for (inner = 0; inner < size; inner++)
            {
              a = 255;
              r = ((cmap_h[y][(int)(inner * 255.0 / size)] >> 16) & 0xff);
              g = ((cmap_h[y][(int)(inner * 255.0 / size)] >> 8) & 0xff);
              b = (cmap_h[y][(int)(inner * 255.0 / size)] & 0xff);

              colormap[inner * size + outer] = (a << 24) + (b << 16) + (g << 8) + (r);
            }
        }
    }

  else if ((x >= 0 && y >= 0) || (x < 0 && y < 0))
    {
      if (x < 0 && y < 0)
        {
          x = y = 0;
        }
      gr_setcolormap(x);
      for (outer = 0; outer < size; outer++)
        {
          for (inner = 0; inner < size; inner++)
            {
              a = 255;
              r1 = ((cmap_h[x][(int)(inner * 255.0 / size)] >> 16) & 0xff);
              g1 = ((cmap_h[x][(int)(inner * 255.0 / size)] >> 8) & 0xff);
              b1 = (cmap_h[x][(int)(inner * 255.0 / size)] & 0xff);

              r2 = ((cmap_h[y][(int)(outer * 255.0 / size)] >> 16) & 0xff);
              g2 = ((cmap_h[y][(int)(outer * 255.0 / size)] >> 8) & 0xff);
              b2 = (cmap_h[y][(int)(outer * 255.0 / size)] & 0xff);

              colormap[outer * size + inner] =
                  (a << 24) + (((b1 + b2) / 2) << 16) + (((g1 + g2) / 2) << 8) + ((r1 + r2) / 2);
            }
        }
    }
}

static void markerHelper(const std::shared_ptr<GRM::Element> &element, const std::shared_ptr<GRM::Context> &context,
                         const std::string &str)
{
  /*!
   * Helperfunction for marker functions using vectors for marker parameters
   *
   * \param[in] element The GRM::Element that contains all marker attributes and data keys. If element's parent is a
   * group element it may fallback to its marker attributes
   * \param[in] context The GRM::Context that contains the actual data
   * \param[in] str The std::string that specifies what GRM Routine should be called (polymarker)
   *
   */
  std::vector<int> type, colorind;
  std::vector<double> size;

  auto parent = element->parentElement();
  bool group = parentTypes.count(parent->localName());
  int skipColorInd = -1000;

  auto attr = element->getAttribute("markertypes");
  if (attr.isString())
    {
      type = GRM::get<std::vector<int>>((*context)[static_cast<std::string>(attr)]);
    }
  else if (group)
    {
      attr = parent->getAttribute("markertypes");
      if (attr.isString())
        {
          type = GRM::get<std::vector<int>>((*context)[static_cast<std::string>(attr)]);
        }
    }

  attr = element->getAttribute("markercolorinds");
  if (attr.isString())
    {
      colorind = GRM::get<std::vector<int>>((*context)[static_cast<std::string>(attr)]);
    }
  else if (group)
    {
      attr = parent->getAttribute("markercolorinds");
      if (attr.isString())
        {
          colorind = GRM::get<std::vector<int>>((*context)[static_cast<std::string>(attr)]);
        }
    }

  attr = element->getAttribute("markersizes");
  if (attr.isString())
    {
      size = GRM::get<std::vector<double>>((*context)[static_cast<std::string>(attr)]);
    }
  else if (group)
    {
      attr = parent->getAttribute("markersizes");
      if (attr.isString())
        {
          size = GRM::get<std::vector<double>>((*context)[static_cast<std::string>(attr)]);
        }
    }

  auto x = static_cast<std::string>(element->getAttribute("x"));
  auto y = static_cast<std::string>(element->getAttribute("y"));
  std::string z;
  if (element->hasAttribute("z"))
    {
      z = static_cast<std::string>(element->getAttribute("z"));
    }

  std::vector<double> x_vec = GRM::get<std::vector<double>>((*context)[x]);
  std::vector<double> y_vec = GRM::get<std::vector<double>>((*context)[y]);
  std::vector<double> z_vec;
  if (auto z_ptr = GRM::get_if<std::vector<double>>((*context)[z]))
    {
      z_vec = *z_ptr;
    }

  auto n = std::min<int>(x_vec.size(), y_vec.size());

  for (int i = 0; i < n; ++i)
    {
      //! fallback to the last element when lists are too short;
      if (!type.empty())
        {
          if (type.size() > i)
            {
              gr_setmarkertype(type[i]);
            }
          else
            {
              gr_setmarkertype(type.back());
            }
        }
      if (!colorind.empty())
        {
          if (colorind.size() > i)
            {
              if (colorind[i] == skipColorInd)
                {
                  continue;
                }
              gr_setmarkercolorind(colorind[i]);
            }
          else
            {
              if (colorind.back() == skipColorInd)
                {
                  continue;
                }
              gr_setmarkercolorind(colorind.back());
            }
        }
      if (!size.empty())
        {
          if (size.size() > i)
            {
              gr_setmarkersize(size[i]);
            }
          else
            {
              gr_setmarkersize(size.back());
            }
        }

      if (str == "polymarker")
        {
          if (redrawws) gr_polymarker(1, (double *)&(x_vec[i]), (double *)&(y_vec[i]));
        }
      else if (str == "polymarker3d")
        {
          if (redrawws) gr_polymarker3d(1, (double *)&(x_vec[i]), (double *)&(y_vec[i]), (double *)&(z_vec[i]));
        }
    }
}

static void lineHelper(const std::shared_ptr<GRM::Element> &element, const std::shared_ptr<GRM::Context> &context,
                       const std::string &str)
{
  /*!
   * Helperfunction for line functions using vectors for line parameters
   *
   * \param[in] element The GRM::Element that contains the attributes and data keys
   * \param[in] context The GRM::Context that contains the actual data
   * \param[in] str The std::string that specifies what GRM Routine should be called (polyline)
   *
   *
   */
  std::vector<int> type, colorind;
  std::vector<double> width;
  std::string z;

  auto parent = element->parentElement();
  bool group = parentTypes.count(parent->localName());

  auto attr = element->getAttribute("linetypes");
  if (attr.isString())
    {
      type = GRM::get<std::vector<int>>((*context)[static_cast<std::string>(attr)]);
    }
  else if (group)
    {
      attr = parent->getAttribute("linetypes");
      if (attr.isString())
        {
          type = GRM::get<std::vector<int>>((*context)[static_cast<std::string>(attr)]);
        }
    }

  attr = element->getAttribute("linecolorinds");
  if (attr.isString())
    {
      colorind = GRM::get<std::vector<int>>((*context)[static_cast<std::string>(attr)]);
    }
  else if (group)
    {
      attr = parent->getAttribute("linecolorinds");
      if (attr.isString())
        {
          colorind = GRM::get<std::vector<int>>((*context)[static_cast<std::string>(attr)]);
        }
    }

  attr = element->getAttribute("linewidths");
  if (attr.isString())
    {
      width = GRM::get<std::vector<double>>((*context)[static_cast<std::string>(attr)]);
    }
  else if (group)
    {
      attr = parent->getAttribute("linewidths");
      if (attr.isString())
        {
          width = GRM::get<std::vector<double>>((*context)[static_cast<std::string>(attr)]);
        }
    }

  auto x = static_cast<std::string>(element->getAttribute("x"));
  auto y = static_cast<std::string>(element->getAttribute("y"));
  if (element->hasAttribute("z"))
    {
      z = static_cast<std::string>(element->getAttribute("z"));
    }

  std::vector<double> x_vec = GRM::get<std::vector<double>>((*context)[x]);
  std::vector<double> y_vec = GRM::get<std::vector<double>>((*context)[y]);
  std::vector<double> z_vec;

  if (auto z_ptr = GRM::get_if<std::vector<double>>((*context)[z]))
    {
      z_vec = *z_ptr;
    }

  int n = std::min<int>(x_vec.size(), y_vec.size());
  for (int i = 0; i < n; ++i)
    {
      if (!type.empty())
        {
          if (type.size() > i)
            {
              gr_setlinetype(type[i]);
            }
          else
            {
              gr_setlinetype(type.back());
            }
        }
      if (!colorind.empty())
        {
          if (colorind.size() > i)
            {
              gr_setlinecolorind(colorind[i]);
            }
          else
            {
              gr_setlinecolorind(colorind.back());
            }
        }
      if (!width.empty())
        {
          if (width.size() > i)
            {
              gr_setlinewidth(width[i]);
            }
          else
            {
              gr_setlinewidth(width.back());
            }
        }
      if (str == "polyline")
        {
          if (redrawws) gr_polyline(2, (double *)&(x_vec[i]), (double *)&(y_vec[i]));
        }
      else if (str == "polyline3d")
        {
          if (redrawws) gr_polyline3d(2, (double *)&(x_vec[i]), (double *)&(y_vec[i]), (double *)&(z_vec[i]));
        }
    }
}

static std::shared_ptr<GRM::Element> getSubplotElement(const std::shared_ptr<GRM::Element> &element)
{
  auto ancestor = element;

  while (ancestor->localName() != "figure")
    {
      bool ancestorIsSubplotGroup =
          (ancestor->hasAttribute("plotGroup") && static_cast<int>(ancestor->getAttribute("plotGroup")));
      if (ancestor->localName() == "layout_gridelement" || ancestorIsSubplotGroup)
        {
          return ancestor;
        }
      else
        {
          ancestor = ancestor->parentElement();
        }
    }
  return nullptr;
}

static void getTickSize(const std::shared_ptr<GRM::Element> &element, double &tick_size)
{
  if (element->hasAttribute("tick_size"))
    {
      tick_size = static_cast<double>(element->getAttribute("tick_size"));
    }
  else
    {
      double viewport[4];
      auto subplot_element = getSubplotElement(element);
      viewport[0] = (double)subplot_element->getAttribute("viewport_xmin");
      viewport[1] = (double)subplot_element->getAttribute("viewport_xmax");
      viewport[2] = (double)subplot_element->getAttribute("viewport_ymin");
      viewport[3] = (double)subplot_element->getAttribute("viewport_ymax");

      double diag = std::sqrt((viewport[1] - viewport[0]) * (viewport[1] - viewport[0]) +
                              (viewport[3] - viewport[2]) * (viewport[3] - viewport[2]));

      tick_size = 0.0075 * diag;
    }
}

static void getMajorCount(const std::shared_ptr<GRM::Element> &element, const std::string kind, int &major_count)
{
  if (element->hasAttribute("major"))
    {
      major_count = static_cast<int>(element->getAttribute("major"));
    }
  else
    {
      if (str_equals_any(kind.c_str(), 9, "wireframe", "surface", "plot3", "scatter3", "polar", "trisurf",
                         "polar_heatmap", "nonuniformpolar_heatmap", "volume"))
        {
          major_count = 2;
        }
      else
        {
          major_count = 5;
        }
    }
}

static void getAxesInformation(const std::shared_ptr<GRM::Element> &element, std::string x_org_pos,
                               std::string y_org_pos, double &x_org, double &y_org, int &x_major, int &y_major,
                               double &x_tick, double &y_tick)
{
  double x_org_low, x_org_high;
  double y_org_low, y_org_high;
  int major_count;

  auto draw_axes_group = element->parentElement();
  auto subplot_element = getSubplotElement(element);
  std::string kind = static_cast<std::string>(subplot_element->getAttribute("kind"));
  int scale = static_cast<int>(subplot_element->getAttribute("scale"));
  double xmin = static_cast<double>(subplot_element->getAttribute("window_xmin"));
  double xmax = static_cast<double>(subplot_element->getAttribute("window_xmax"));
  double ymin = static_cast<double>(subplot_element->getAttribute("window_ymin"));
  double ymax = static_cast<double>(subplot_element->getAttribute("window_ymax"));

  getMajorCount(element, kind, major_count);

  if (element->hasAttribute("x_major"))
    {
      x_major = static_cast<int>(element->getAttribute("x_major"));
    }
  else
    {
      if (!(scale & GR_OPTION_X_LOG))
        {
          if (draw_axes_group->hasAttribute("xticklabels"))
            {
              x_major = 0;
            }
          else if (kind == "barplot")
            {
              x_major = 1;
            }
          else
            {
              x_major = major_count;
            }
        }
      else
        {
          x_major = 1;
        }
      element->setAttribute("x_major", x_major);
    }

  if (element->hasAttribute("x_tick"))
    {
      x_tick = static_cast<double>(element->getAttribute("x_tick"));
    }
  else
    {
      if (!(scale & GR_OPTION_X_LOG))
        {
          if (kind == "barplot")
            {
              x_tick = 1;
            }
          else
            {
              if (x_major != 0)
                {
                  x_tick = auto_tick(xmin, xmax) / x_major;
                }
              else
                {
                  x_tick = 1;
                }
            }
        }
      else
        {
          x_tick = 1;
        }
    }

  if (element->hasAttribute("x_org"))
    {
      x_org = static_cast<double>(element->getAttribute("x_org"));
    }
  else
    {
      if (!(scale & GR_OPTION_FLIP_X))
        {
          x_org_low = xmin;
          x_org_high = xmax;
        }
      else
        {
          x_org_low = xmax;
          x_org_high = xmin;
        }
      if (x_org_pos == "low")
        {
          x_org = x_org_low;
        }
      else
        {
          x_org = x_org_high;
          x_major = -x_major;
        }
    }

  if (element->hasAttribute("y_major"))
    {
      y_major = static_cast<int>(element->getAttribute("y_major"));
    }
  else
    {
      if (!(scale & GR_OPTION_Y_LOG))
        {
          if (draw_axes_group->hasAttribute("yticklabels"))
            {
              y_major = 0;
            }
          else
            {
              y_major = major_count;
            }
        }
      else
        {
          y_major = 1;
        }
      element->setAttribute("y_major", y_major);
    }

  if (element->hasAttribute("y_tick"))
    {
      y_tick = static_cast<double>(element->getAttribute("y_tick"));
    }
  else
    {
      if (!(scale & GR_OPTION_Y_LOG))
        {
          if (y_major != 0)
            {
              y_tick = auto_tick(ymin, ymax) / y_major;
            }
          else
            {
              y_tick = 1;
            }
        }
      else
        {
          y_tick = 1;
        }
    }

  if (element->hasAttribute("y_org"))
    {
      y_org = static_cast<double>(element->getAttribute("y_org"));
    }
  else
    {
      if (!(scale & GR_OPTION_FLIP_Y))
        {
          y_org_low = ymin;
          y_org_high = ymax;
        }
      else
        {
          y_org_low = ymax;
          y_org_high = ymin;
        }
      if (y_org_pos == "low")
        {
          y_org = y_org_low;
        }
      else
        {
          y_org = y_org_high;
          y_major = -y_major;
        }
    }
}

static void getAxes3dInformation(const std::shared_ptr<GRM::Element> &element, std::string x_org_pos,
                                 std::string y_org_pos, std::string z_org_pos, double &x_org, double &y_org,
                                 double &z_org, int &x_major, int &y_major, int &z_major, double &x_tick,
                                 double &y_tick, double &z_tick)
{
  getAxesInformation(element, x_org_pos, y_org_pos, x_org, y_org, x_major, y_major, x_tick, y_tick);

  double z_org_low, z_org_high;
  int major_count;

  auto draw_axes_group = element->parentElement();
  auto subplot_element = getSubplotElement(element);
  std::string kind = static_cast<std::string>(subplot_element->getAttribute("kind"));
  int scale = static_cast<int>(subplot_element->getAttribute("scale"));
  double zmin = static_cast<double>(subplot_element->getAttribute("window_zmin"));
  double zmax = static_cast<double>(subplot_element->getAttribute("window_zmax"));

  getMajorCount(element, kind, major_count);

  if (element->hasAttribute("z_major"))
    {
      z_major = static_cast<int>(element->getAttribute("z_major"));
    }
  else
    {
      if (!(scale & GR_OPTION_Z_LOG))
        {
          z_major = major_count;
        }
      else
        {
          z_major = 1;
        }
    }

  if (element->hasAttribute("z_tick"))
    {
      z_tick = static_cast<double>(element->getAttribute("z_tick"));
    }
  else
    {
      if (!(scale & GR_OPTION_Z_LOG))
        {
          if (z_major != 0)
            {
              z_tick = auto_tick(zmin, zmax) / z_major;
            }
          else
            {
              z_tick = 1;
            }
        }
      else
        {
          z_tick = 1;
        }
    }

  if (element->hasAttribute("z_org"))
    {
      z_org = static_cast<double>(element->getAttribute("z_org"));
    }
  else
    {
      if (!(scale & GR_OPTION_FLIP_Z))
        {
          z_org_low = zmin;
          z_org_high = zmax;
        }
      else
        {
          z_org_low = zmax;
          z_org_high = zmin;
        }
      if (z_org_pos == "low")
        {
          z_org = z_org_low;
        }
      else
        {
          z_org = z_org_high;
          z_major = -z_major;
        }
    }
}

/*!
 * Draw an xticklabel at the specified position while trying to stay in the available space.
 * Every space character (' ') is seen as a possible position to break the label into the next line.
 * The label will not break into the next line when enough space is available.
 * If a label or a part of it does not fit in the available space but doesnt have a space character to break it up
 * it will get drawn anyway.
 *
 * \param[in] x The X coordinate of starting position of the label.
 * \param[in] y The Y coordinate of starting position of the label.
 * \param[in] label The label to be drawn.
 * \param[in] available_width The available space in X direction around the starting position.
 */
void draw_xticklabel(double x, double y, const char *label, double available_width,
                     const std::shared_ptr<GRM::Element> &element, bool init, int child_id)
{
  char new_label[256];
  int breakpoint_positions[128];
  int cur_num_breakpoints = 0;
  int i = 0;
  int cur_start = 0;
  double tbx[4], tby[4];
  double width;
  double charheight;
  del_values del = del_values::update_without_default;
  std::shared_ptr<GRM::Element> text;

  gr_inqcharheight(&charheight);

  del = del_values(static_cast<int>(element->getAttribute("_delete_children")));

  for (i = 0; i == 0 || label[i - 1] != '\0'; ++i)
    {
      if (label[i] == ' ' || label[i] == '\0')
        {
          /* calculate width of the next part of the label to be drawn */
          new_label[i] = '\0';
          gr_inqtext(x, y, new_label + cur_start, tbx, tby);
          gr_wctondc(&tbx[0], &tby[0]);
          gr_wctondc(&tbx[1], &tby[1]);
          width = tbx[1] - tbx[0];
          new_label[i] = ' ';

          /* add possible breakpoint */
          breakpoint_positions[cur_num_breakpoints++] = i;

          if (width > available_width)
            {
              /* part is too big but doesn't have a breakpoint in it */
              if (cur_num_breakpoints == 1)
                {
                  new_label[i] = '\0';
                }
              else /* part is too big and has breakpoints in it */
                {
                  /* break label at last breakpoint that still allowed the text to fit */
                  new_label[breakpoint_positions[cur_num_breakpoints - 2]] = '\0';
                }

              if ((del != del_values::update_without_default && del != del_values::update_with_default) || init)
                {
                  text = global_render->createText(x, y, new_label + cur_start);
                  element->append(text);
                  text->setAttribute("_child_id", child_id++);
                }
              else
                {
                  text = element->querySelectors("[_child_id=" + std::to_string(child_id++) + "]");
                  if (text != nullptr)
                    global_render->createText(x, y, new_label + cur_start, CoordinateSpace::NDC, text);
                }
              if (text != nullptr) text->setAttribute("name", "xtick");

              if (cur_num_breakpoints == 1)
                {
                  cur_start = i + 1;
                  cur_num_breakpoints = 0;
                }
              else
                {
                  cur_start = breakpoint_positions[cur_num_breakpoints - 2] + 1;
                  breakpoint_positions[0] = breakpoint_positions[cur_num_breakpoints - 1];
                  cur_num_breakpoints = 1;
                }
              y -= charheight * 1.5;
            }
        }
      else
        {
          new_label[i] = label[i];
        }
    }

  /* 0-terminate the new label */
  new_label[i] = '\0';

  /* draw the rest */
  if ((del != del_values::update_without_default && del != del_values::update_with_default) || init)
    {
      text = global_render->createText(x, y, std::string(new_label + cur_start));
      element->append(text);
      text->setAttribute("_child_id", child_id++);
    }
  else
    {
      text = element->querySelectors("[_child_id=" + std::to_string(child_id++) + "]");
      if (text != nullptr)
        global_render->createText(x, y, std::string(new_label + cur_start), CoordinateSpace::NDC, text);
    }
  if (text != nullptr) text->setAttribute("name", "xtick");
}

/*!
 * Draw an yticklabel at the specified position while trying to stay in the available space.
 * Every space character (' ') is seen as a possible position to break the label into the next line.
 * The label will not break into the next line when enough space is available.
 * If a label or a part of it does not fit in the available space but doesnt have a space character to break it up
 * it will get drawn anyway.
 *
 * \param[in] x The X coordinate of starting position of the label.
 * \param[in] y The Y coordinate of starting position of the label.
 * \param[in] label The label to be drawn.
 * \param[in] available_height The available space in Y direction around the starting position.
 */
void draw_yticklabel(double x, double y, const char *label, double available_height,
                     const std::shared_ptr<GRM::Element> &element, bool init, int child_id)
{
  char new_label[256];
  int breakpoint_positions[128];
  int cur_num_breakpoints = 0;
  int i = 0;
  int cur_start = 0;
  double tbx[4], tby[4];
  double height;
  double charheight;
  del_values del = del_values::update_without_default;
  std::shared_ptr<GRM::Element> text;

  gr_inqcharheight(&charheight);

  del = del_values(static_cast<int>(element->getAttribute("_delete_children")));

  for (i = 0; i == 0 || label[i - 1] != '\0'; ++i)
    {
      if (label[i] == ' ' || label[i] == '\0')
        {
          /* calculate width of the next part of the label to be drawn */
          new_label[i] = '\0';
          gr_inqtext(x, y, new_label + cur_start, tbx, tby);
          gr_wctondc(&tbx[0], &tby[0]);
          gr_wctondc(&tbx[1], &tby[1]);
          height = tby[1] - tby[0];
          new_label[i] = ' ';

          /* add possible breakpoint */
          breakpoint_positions[cur_num_breakpoints++] = i;

          if (height > available_height)
            {
              /* part is too big but doesn't have a breakpoint in it */
              if (cur_num_breakpoints == 1)
                {
                  new_label[i] = '\0';
                }
              else /* part is too big and has breakpoints in it */
                {
                  /* break label at last breakpoint that still allowed the text to fit */
                  new_label[breakpoint_positions[cur_num_breakpoints - 2]] = '\0';
                }

              if ((del != del_values::update_without_default && del != del_values::update_with_default) || init)
                {
                  text = global_render->createText(x, y, new_label + cur_start);
                  element->append(text);
                  text->setAttribute("_child_id", child_id++);
                }
              else
                {
                  text = element->querySelectors("[_child_id=" + std::to_string(child_id++) + "]");
                  if (text != nullptr)
                    global_render->createText(x, y, new_label + cur_start, CoordinateSpace::NDC, text);
                }
              if (text != nullptr) text->setAttribute("name", "ytick");

              if (cur_num_breakpoints == 1)
                {
                  cur_start = i + 1;
                  cur_num_breakpoints = 0;
                }
              else
                {
                  cur_start = breakpoint_positions[cur_num_breakpoints - 2] + 1;
                  breakpoint_positions[0] = breakpoint_positions[cur_num_breakpoints - 1];
                  cur_num_breakpoints = 1;
                }
            }
        }
      else
        {
          new_label[i] = label[i];
        }
    }

  /* 0-terminate the new label */
  new_label[i] = '\0';

  /* draw the rest */
  if ((del != del_values::update_without_default && del != del_values::update_with_default) || init)
    {
      text = global_render->createText(x, y, std::string(new_label + cur_start));
      element->append(text);
      text->setAttribute("_child_id", child_id++);
    }
  else
    {
      text = element->querySelectors("[_child_id=" + std::to_string(child_id++) + "]");
      if (text != nullptr)
        global_render->createText(x, y, std::string(new_label + cur_start), CoordinateSpace::NDC, text);
    }
  if (text != nullptr) text->setAttribute("name", "ytick");
}

static void legend_size(const std::shared_ptr<GRM::Element> &element, double *w, double *h)
{
  double tbx[4], tby[4];
  int labelsExist = 1;
  unsigned int num_labels;
  std::vector<std::string> labels;
  *w = 0;
  *h = 0;

  if (auto render = std::dynamic_pointer_cast<GRM::Render>(element->ownerDocument()))
    {
      auto context = render->getContext();
      std::string key = static_cast<std::string>(element->getAttribute("labels"));
      labels = GRM::get<std::vector<std::string>>((*context)[key]);
    }
  if (labelsExist)
    {
      for (auto current_label : labels)
        {
          gr_inqtext(0, 0, current_label.data(), tbx, tby);
          *w = grm_max(*w, tbx[2] - tbx[0]);
          *h += grm_max(tby[2] - tby[0], 0.03);
        }
    }
}

void GRM::Render::get_figure_size(int *pixel_width, int *pixel_height, double *metric_width, double *metric_height)
{
  double display_metric_width, display_metric_height;
  int display_pixel_width, display_pixel_height;
  double dpm[2], dpi[2];
  int tmp_size_i[2], pixel_size[2];
  double tmp_size_d[2], metric_size[2];
  int i;
  std::string size_unit, size_type;
  std::array<std::string, 2> vars = {"x", "y"};
  std::array<double, 2> default_size = {PLOT_DEFAULT_WIDTH, PLOT_DEFAULT_HEIGHT};

  std::shared_ptr<GRM::Element> root = active_figure;

#ifdef __EMSCRIPTEN__
  display_metric_width = 0.16384;
  display_metric_height = 0.12288;
  display_pixel_width = 640;
  display_pixel_height = 480;
#else
  gr_inqdspsize(&display_metric_width, &display_metric_height, &display_pixel_width, &display_pixel_height);
#endif
  dpm[0] = display_pixel_width / display_metric_width;
  dpm[1] = display_pixel_height / display_metric_height;
  dpi[0] = dpm[0] * 0.0254;
  dpi[1] = dpm[1] * 0.0254;

  /* TODO: Overwork this calculation */
  if (root->hasAttribute("figsize_x") && root->hasAttribute("figsize_y"))
    {
      tmp_size_d[0] = (double)root->getAttribute("figsize_x");
      tmp_size_d[1] = (double)root->getAttribute("figsize_y");
      for (i = 0; i < 2; ++i)
        {
          pixel_size[i] = (int)grm_round(tmp_size_d[i] * dpi[i]);
          metric_size[i] = tmp_size_d[i] / 0.0254;
        }
    }
  else if (root->hasAttribute("size_x") && root->hasAttribute("size_y"))
    {
      for (i = 0; i < 2; ++i)
        {
          size_unit = (std::string)root->getAttribute("size_" + vars[i] + "_unit");
          size_type = (std::string)root->getAttribute("size_" + vars[i] + "_type");
          if (size_unit.empty()) size_unit = "px";
          tmp_size_d[i] = default_size[i];

          if (size_type == "double" || size_type == "int")
            {
              tmp_size_d[i] = (double)root->getAttribute("size_" + vars[i]);
              auto meters_per_unit_iter = symbol_to_meters_per_unit.find(size_unit);
              if (meters_per_unit_iter != symbol_to_meters_per_unit.end())
                {
                  double meters_per_unit = meters_per_unit_iter->second;
                  double pixels_per_unit = meters_per_unit * dpm[i];

                  tmp_size_d[i] *= pixels_per_unit;
                }
            }
          pixel_size[i] = (int)grm_round(tmp_size_d[i]);
          metric_size[i] = tmp_size_d[i] / dpm[i];
        }
    }
  else
    {
      pixel_size[0] = (int)grm_round(PLOT_DEFAULT_WIDTH);
      pixel_size[1] = (int)grm_round(PLOT_DEFAULT_HEIGHT);
      metric_size[0] = PLOT_DEFAULT_WIDTH / dpm[0];
      metric_size[1] = PLOT_DEFAULT_HEIGHT / dpm[1];
    }

  if (pixel_width != nullptr)
    {
      *pixel_width = pixel_size[0];
    }
  if (pixel_height != nullptr)
    {
      *pixel_height = pixel_size[1];
    }
  if (metric_width != nullptr)
    {
      *metric_width = metric_size[0];
    }
  if (metric_height != nullptr)
    {
      *metric_height = metric_size[1];
    }
}

static void legend_size(std::vector<std::string> labels, double *w, double *h)
{
  double tbx[4], tby[4];
  unsigned int num_labels;

  *w = 0;
  *h = 0;

  if (!labels.empty())
    {
      for (std::string current_label : labels)
        {
          gr_inqtext(0, 0, current_label.data(), tbx, tby);
          *w = grm_max(*w, tbx[2] - tbx[0]);
          *h += grm_max(tby[2] - tby[0], 0.03);
        }
    }
}

static void setNextColor(gr_color_type_t color_type, std::vector<int> &color_indices,
                         std::vector<double> &color_rgb_values, const std::shared_ptr<GRM::Element> &element)
{
  // TODO: is this method really needed? Cant it be replaced by set_next_color?
  const static std::vector<int> fallback_color_indices{989, 982, 980, 981, 996, 983, 995, 988, 986, 990,
                                                       991, 984, 992, 993, 994, 987, 985, 997, 998, 999};
  static double saved_color[3];
  static int last_array_index = -1;
  static unsigned int color_array_length = -1;
  int current_array_index = last_array_index + 1;
  int color_index = 0;
  int reset = (color_type == GR_COLOR_RESET);
  int gks_errind = GKS_K_NO_ERROR;

  if (reset)
    {
      last_array_index = -1;
      color_array_length = -1;
      return;
    }

  if (color_indices.empty() && color_rgb_values.empty())
    {
      color_indices = fallback_color_indices;
    }

  if (last_array_index < 0 && !color_rgb_values.empty())
    {
      gks_inq_color_rep(1, PLOT_CUSTOM_COLOR_INDEX, GKS_K_VALUE_SET, &gks_errind, &saved_color[0], &saved_color[1],
                        &saved_color[2]);
    }

  current_array_index %= color_array_length;

  if (!color_indices.empty())
    {
      color_index = color_indices[current_array_index];
      last_array_index = current_array_index;
    }
  else if (!color_rgb_values.empty())
    {
      color_index = PLOT_CUSTOM_COLOR_INDEX;
      global_render->setColorRep(element, PLOT_CUSTOM_COLOR_INDEX, color_rgb_values[current_array_index],
                                 color_rgb_values[current_array_index + 1], color_rgb_values[current_array_index + 2]);
      last_array_index = current_array_index + 2;
    }

  if (color_type & GR_COLOR_LINE)
    {
      global_render->setLineColorInd(element, color_index);
    }
  if (color_type & GR_COLOR_MARKER)
    {
      global_render->setMarkerColorInd(element, color_index);
    }
  if (color_type & GR_COLOR_FILL)
    {
      global_render->setFillColorInd(element, color_index);
    }
  if (color_type & GR_COLOR_TEXT)
    {
      global_render->setTextColorInd(element, color_index);
    }
  if (color_type & GR_COLOR_BORDER)
    {
      global_render->setBorderColorInd(element, color_index);
    }
}

void receiverfunction(int id, double x_min, double x_max, double y_min, double y_max)
{
  if (!(x_min == DBL_MAX || x_max == -DBL_MAX || y_min == DBL_MAX || y_max == -DBL_MAX))
    {
      bounding_map[id]->setAttribute("_bbox_id", id);
      bounding_map[id]->setAttribute("_bbox_xmin", x_min);
      bounding_map[id]->setAttribute("_bbox_xmax", x_max);
      bounding_map[id]->setAttribute("_bbox_ymin", y_min);
      bounding_map[id]->setAttribute("_bbox_ymax", y_max);
    }
}

static bool getLimitsForColorbar(const std::shared_ptr<GRM::Element> &element, double &c_min, double &c_max)
{
  bool limits_found = true;
  auto plot_parent = element->parentElement();
  getPlotParent(plot_parent);

  if (!std::isnan(static_cast<double>(plot_parent->getAttribute("_clim_min"))) &&
      !std::isnan(static_cast<double>(plot_parent->getAttribute("_clim_max"))))
    {
      c_min = static_cast<double>(plot_parent->getAttribute("_clim_min"));
      c_max = static_cast<double>(plot_parent->getAttribute("_clim_max"));
    }
  else if (!std::isnan(static_cast<double>(plot_parent->getAttribute("_zlim_min"))) &&
           !std::isnan(static_cast<double>(plot_parent->getAttribute("_zlim_max"))))
    {
      c_min = static_cast<double>(plot_parent->getAttribute("_zlim_min"));
      c_max = static_cast<double>(plot_parent->getAttribute("_zlim_max"));
    }
  else
    {
      limits_found = false;
    }

  return limits_found;
}

static double find_max_step(unsigned int n, std::vector<double> x)
{
  double max_step = 0.0;
  unsigned int i;

  if (n < 2)
    {
      return 0.0;
    }
  for (i = 1; i < n; ++i)
    {
      max_step = grm_max(x[i] - x[i - 1], max_step);
    }

  return max_step;
}

static void extendErrorbars(const std::shared_ptr<GRM::Element> &element, const std::shared_ptr<GRM::Context> &context,
                            std::vector<double> x, std::vector<double> y)
{
  int id = static_cast<int>(global_root->getAttribute("_id"));
  std::string str = std::to_string(id);
  global_root->setAttribute("_id", ++id);

  (*context)["x" + str] = x;
  element->setAttribute("x", "x" + str);
  (*context)["y" + str] = y;
  element->setAttribute("y", "y" + str);
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~ attribute processing functions ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

static void processAlpha(const std::shared_ptr<GRM::Element> &element)
{
  gr_settransparency(static_cast<double>(element->getAttribute("alpha")));
}

static void processBorderColorInd(const std::shared_ptr<GRM::Element> &element)
{
  gr_setbordercolorind(static_cast<int>(element->getAttribute("bordercolorind")));
}

static void processCalcWindowAndViewportFromParent(const std::shared_ptr<GRM::Element> &element)
{
  std::string kind = static_cast<std::string>(element->getAttribute("kind"));
  double viewport[4];
  double x_min, x_max, y_min, y_max, c_min, c_max;

  if (element->parentElement()->hasAttribute("marginalheatmap_kind"))
    {
      std::string orientation = static_cast<std::string>(element->getAttribute("orientation"));
      auto plot_group = element->parentElement()->parentElement();
      viewport[0] = static_cast<double>(plot_group->getAttribute("viewport_xmin"));
      viewport[1] = static_cast<double>(plot_group->getAttribute("viewport_xmax"));
      viewport[2] = static_cast<double>(plot_group->getAttribute("viewport_ymin"));
      viewport[3] = static_cast<double>(plot_group->getAttribute("viewport_ymax"));
      x_min = static_cast<double>(plot_group->getAttribute("_xlim_min"));
      x_max = static_cast<double>(plot_group->getAttribute("_xlim_max"));
      y_min = static_cast<double>(plot_group->getAttribute("_ylim_min"));
      y_max = static_cast<double>(plot_group->getAttribute("_ylim_max"));
      if (!std::isnan(static_cast<double>(plot_group->getAttribute("_clim_min"))))
        {
          c_min = static_cast<double>(plot_group->getAttribute("_clim_min"));
        }
      else
        {
          c_min = static_cast<double>(plot_group->getAttribute("_zlim_min"));
        }
      if (!std::isnan(static_cast<double>(plot_group->getAttribute("_clim_max"))))
        {
          c_max = static_cast<double>(plot_group->getAttribute("_clim_max"));
        }
      else
        {
          c_max = static_cast<double>(plot_group->getAttribute("_zlim_max"));
        }

      if (orientation == "vertical")
        {
          gr_setwindow(0.0, c_max / 10, y_min, y_max);
          gr_setviewport(viewport[1] + 0.02 + 0.0, viewport[1] + 0.12 + 0.0, viewport[2], viewport[3]);
        }
      else if (orientation == "horizontal")
        {
          gr_setwindow(x_min, x_max, 0.0, c_max / 10);
          gr_setviewport(viewport[0], viewport[1], viewport[3] + 0.02, grm_min(viewport[3] + 0.12, 1));
        }
    }
}

static void processCharExpan(const std::shared_ptr<GRM::Element> &element)
{
  gr_setcharexpan(static_cast<double>(element->getAttribute("charexpan")));
}

static void processCharHeight(const std::shared_ptr<GRM::Element> &element)
{
  gr_setcharheight(static_cast<double>(element->getAttribute("charheight")));
}

static void processCharSpace(const std::shared_ptr<GRM::Element> &element)
{
  gr_setcharspace(static_cast<double>(element->getAttribute("charspace")));
}

static void processCharUp(const std::shared_ptr<GRM::Element> &element)
{
  gr_setcharup(static_cast<double>(element->getAttribute("charup_x")),
               static_cast<double>(element->getAttribute("charup_y")));
}

static void processClipXForm(const std::shared_ptr<GRM::Element> &element)
{
  gr_selectclipxform(static_cast<double>(element->getAttribute("clipxform")));
}

static void processColorbarPosition(const std::shared_ptr<GRM::Element> &element)
{
  double viewport[4];

  auto subplot_element = getSubplotElement(element);

  double width = static_cast<double>(element->getAttribute("width"));
  double offset = static_cast<double>(element->getAttribute("offset"));

  if (!subplot_element->hasAttribute("viewport_xmin") || !subplot_element->hasAttribute("viewport_xmax") ||
      !subplot_element->hasAttribute("viewport_ymin") || !subplot_element->hasAttribute("viewport_ymax"))
    {
      throw NotFoundError("Missing viewport\n");
    }

  viewport[0] = static_cast<double>(subplot_element->getAttribute("viewport_xmin"));
  viewport[1] = static_cast<double>(subplot_element->getAttribute("viewport_xmax"));
  viewport[2] = static_cast<double>(subplot_element->getAttribute("viewport_ymin"));
  viewport[3] = static_cast<double>(subplot_element->getAttribute("viewport_ymax"));

  gr_setviewport(viewport[1] + offset, viewport[1] + offset + width, viewport[2], viewport[3]);
}

static void processColormap(const std::shared_ptr<GRM::Element> &element)
{
  int colormap;

  gr_setcolormap(static_cast<int>(element->getAttribute("colormap")));
  /* TODO: Implement other datatypes for `colormap` */
}

static void processColorRep(const std::shared_ptr<GRM::Element> &element, const std::string attribute)
{
  int index, hex_int;
  double red, green, blue;
  std::string name, hex_string;
  std::stringstream stringstream;

  auto end = attribute.find('.');
  index = std::stoi(attribute.substr(end + 1, attribute.size()));

  hex_int = 0;
  hex_string = static_cast<std::string>(element->getAttribute(attribute));
  stringstream << std::hex << hex_string;
  stringstream >> hex_int;

  red = ((hex_int >> 16) & 0xFF) / 255.0;
  green = ((hex_int >> 8) & 0xFF) / 255.0;
  blue = ((hex_int)&0xFF) / 255.0;

  gr_setcolorrep(index, red, green, blue);
}

static void processColorReps(const std::shared_ptr<GRM::Element> &element)
{
  for (auto &attr : element->getAttributeNames())
    {
      auto start = 0U;
      auto end = attr.find('.');
      if (attr.substr(start, end) == "colorrep")
        {
          processColorRep(element, attr);
        }
    }
}

static void processFillColorInd(const std::shared_ptr<GRM::Element> &element)
{
  gr_setfillcolorind(static_cast<int>(element->getAttribute("fillcolorind")));
}

static void processFillIntStyle(const std::shared_ptr<GRM::Element> &element)
{
  gr_setfillintstyle(static_cast<int>(element->getAttribute("fillintstyle")));
}

static void processFillStyle(const std::shared_ptr<GRM::Element> &element)
{
  gr_setfillstyle(static_cast<int>(element->getAttribute("fillstyle")));
}

static void processFlip(const std::shared_ptr<GRM::Element> &element)
{
  if (element->localName() == "colorbar")
    {
      int options;
      int xflip = static_cast<int>(element->getAttribute("xflip"));
      int yflip = static_cast<int>(element->getAttribute("yflip"));
      gr_inqscale(&options);

      if (xflip)
        {
          options = (options | GR_OPTION_FLIP_Y) & ~GR_OPTION_FLIP_X;
        }
      else if (yflip)
        {
          options = options & ~GR_OPTION_FLIP_Y & ~GR_OPTION_FLIP_X;
        }
      else
        {
          options = options & ~GR_OPTION_FLIP_X;
        }
    }
}

static void processFont(const std::shared_ptr<GRM::Element> &element)
{
  int font, font_precision;

  /* `font` and `font_precision` are always set */
  if (element->hasAttribute("font_precision"))
    {
      font = static_cast<int>(element->getAttribute("font"));
      font_precision = static_cast<int>(element->getAttribute("font_precision"));
      logger((stderr, "Using font: %d with precision %d\n", font, font_precision));
      gr_settextfontprec(font, font_precision);
    }
  /* TODO: Implement other datatypes for `font` and `font_precision` */
}

static void processGROptionFlipX(const std::shared_ptr<GRM::Element> &element)
{
  int options;
  int flip_x = static_cast<int>(element->getAttribute("gr_option_flip_x"));
  gr_inqscale(&options);

  if (flip_x)
    {
      gr_setscale(options | GR_OPTION_FLIP_X);
    }
  else
    {
      gr_setscale(options & ~GR_OPTION_FLIP_X);
    }
}

static void processGROptionFlipY(const std::shared_ptr<GRM::Element> &element)
{
  int options;
  int flip_y = static_cast<int>(element->getAttribute("gr_option_flip_y"));
  gr_inqscale(&options);

  if (flip_y)
    {
      gr_setscale(options | GR_OPTION_FLIP_Y);
    }
  else
    {
      gr_setscale(options & ~GR_OPTION_FLIP_Y);
    }
}

static void processGR3BackgroundColor(const std::shared_ptr<GRM::Element> &element)
{
  double r, g, b, a;
  r = (double)element->getAttribute("gr3backgroundcolor_red");
  g = (double)element->getAttribute("gr3backgroundcolor_green");
  b = (double)element->getAttribute("gr3backgroundcolor_blue");
  a = (double)element->getAttribute("gr3backgroundcolor_alpha");

  gr3_setbackgroundcolor(r, g, b, a);
}

static void processGR3CameraLookAt(const std::shared_ptr<GRM::Element> &element)
{
  double camera_x, camera_y, camera_z, center_x, center_y, center_z, up_x, up_y, up_z;

  camera_x = (double)element->getAttribute("gr3cameralookat_camera_x");
  camera_y = (double)element->getAttribute("gr3cameralookat_camera_y");
  camera_z = (double)element->getAttribute("gr3cameralookat_camera_z");
  center_x = (double)element->getAttribute("gr3cameralookat_center_x");
  center_y = (double)element->getAttribute("gr3cameralookat_center_y");
  center_z = (double)element->getAttribute("gr3cameralookat_center_z");
  up_x = (double)element->getAttribute("gr3cameralookat_up_x");
  up_y = (double)element->getAttribute("gr3cameralookat_up_y");
  up_z = (double)element->getAttribute("gr3cameralookat_up_z");

  gr3_cameralookat(camera_x, camera_y, camera_z, center_x, center_y, center_z, up_x, up_y, up_z);
}

static void processMarginalheatmapKind(const std::shared_ptr<GRM::Element> &element)
{
  std::string mkind = static_cast<std::string>(element->getAttribute("marginalheatmap_kind"));
  for (const auto &child : element->children())
    {
      if (!child->hasAttribute("calc_window_and_viewport_from_parent")) continue;
      if (mkind == "line")
        {
          std::shared_ptr<GRM::Context> context;
          std::shared_ptr<GRM::Render> render;
          std::shared_ptr<GRM::Element> line_elem, marker_elem;

          render = std::dynamic_pointer_cast<GRM::Render>(child->ownerDocument());
          if (!render)
            {
              throw NotFoundError("Render-document not found for element\n");
            }
          context = render->getContext();
          auto orientation = static_cast<std::string>(child->getAttribute("orientation"));
          int is_vertical = orientation == "vertical";
          int xind = static_cast<int>(element->getAttribute("xind"));
          int yind = static_cast<int>(element->getAttribute("yind"));

          auto plot_group = element->parentElement();

          double c_min = static_cast<double>(plot_group->getAttribute("_zlim_min"));
          double c_max = static_cast<double>(plot_group->getAttribute("_zlim_max"));

          int i;
          double y_max = 0;
          std::vector<double> z =
              GRM::get<std::vector<double>>((*context)[static_cast<std::string>(element->getAttribute("z"))]);
          std::vector<double> y =
              GRM::get<std::vector<double>>((*context)[static_cast<std::string>(element->getAttribute("y"))]);
          std::vector<double> xi =
              GRM::get<std::vector<double>>((*context)[static_cast<std::string>(child->getAttribute("xi"))]);
          std::vector<double> x =
              GRM::get<std::vector<double>>((*context)[static_cast<std::string>(element->getAttribute("x"))]);

          int y_length = y.size();
          int x_length = xi.size();

          double xmin = static_cast<double>(element->getAttribute("xrange_min"));
          double xmax = static_cast<double>(element->getAttribute("xrange_max"));
          double ymin = static_cast<double>(element->getAttribute("yrange_min"));
          double ymax = static_cast<double>(element->getAttribute("yrange_max"));

          // plot step in marginal
          for (i = 0; i < (is_vertical ? y_length : x_length); i++)
            {
              if (is_vertical)
                {
                  y[i] = std::isnan(z[xind + i * x_length]) ? 0 : z[xind + i * x_length];
                  y_max = grm_max(y_max, y[i]);
                }
              else
                {
                  y[i] = std::isnan(z[x_length * yind + i]) ? 0 : z[x_length * yind + i];
                  y_max = grm_max(y_max, y[i]);
                }
            }
          for (i = 0; i < (is_vertical ? y_length : x_length); i++)
            {
              y[i] = y[i] / y_max * (c_max / 15);
              xi[i] = x[i] + (is_vertical ? ymin : xmin);
            }

          double x_pos, y_pos;
          unsigned int len = is_vertical ? y_length : x_length;
          std::vector<double> x_step_boundaries(2 * len);
          std::vector<double> y_step_values(2 * len);

          x_step_boundaries[0] = is_vertical ? ymin : xmin;
          for (i = 2; i < 2 * len; i += 2)
            {
              x_step_boundaries[i - 1] = x_step_boundaries[i] =
                  x_step_boundaries[0] + (i / 2) * (is_vertical ? (ymax - ymin) : (xmax - xmin)) / len;
            }
          x_step_boundaries[2 * len - 1] = is_vertical ? ymax : xmax;
          y_step_values[0] = y[0];
          for (i = 2; i < 2 * len; i += 2)
            {
              y_step_values[i - 1] = y[i / 2 - 1];
              y_step_values[i] = y[i / 2];
            }
          y_step_values[2 * len - 1] = y[len - 1];

          int id = static_cast<int>(global_root->getAttribute("_id"));
          global_root->setAttribute("_id", id + 1);
          auto id_str = std::to_string(id);

          // special case where it shouldn't be necessary to use the delete attribute from the children
          if (!child->hasChildNodes())
            {
              if (is_vertical)
                {
                  line_elem =
                      global_render->createPolyline("x" + id_str, y_step_values, "y" + id_str, x_step_boundaries);
                  x_pos = (x_step_boundaries[yind * 2] + x_step_boundaries[yind * 2 + 1]) / 2;
                  y_pos = y[yind];
                  marker_elem = global_render->createPolymarker(y_pos, x_pos);
                }
              else
                {
                  line_elem =
                      global_render->createPolyline("x" + id_str, x_step_boundaries, "y" + id_str, y_step_values);
                  x_pos = (x_step_boundaries[xind * 2] + x_step_boundaries[xind * 2 + 1]) / 2;
                  y_pos = y[xind];
                  marker_elem = global_render->createPolymarker(x_pos, y_pos);
                }

              global_render->setLineColorInd(line_elem, 989);
              global_render->setMarkerColorInd(marker_elem, 2);
              global_render->setMarkerType(marker_elem, -1);
              global_render->setMarkerSize(marker_elem, 1.5 * (len / (is_vertical ? (ymax - ymin) : (xmax - xmin))));

              marker_elem->setAttribute("name", "line");
              line_elem->setAttribute("name", "line");
              child->append(marker_elem);
              child->append(line_elem);
              marker_elem->setAttribute("z_index", 2);
            }
          else if (xind == -1 || yind == -1)
            {
              child->remove();
            }
          else
            {
              for (const auto &childchild : child->children())
                {
                  if (childchild->localName() == "polyline")
                    {
                      std::string x_key = "x" + id_str;
                      std::string y_key = "y" + id_str;
                      if (is_vertical)
                        {
                          (*context)[x_key] = y_step_values;
                          (*context)[y_key] = x_step_boundaries;
                        }
                      else
                        {
                          (*context)[y_key] = y_step_values;
                          (*context)[x_key] = x_step_boundaries;
                        }
                      childchild->setAttribute("x", x_key);
                      childchild->setAttribute("y", y_key);
                    }
                  else if (childchild->localName() == "polymarker")
                    {
                      if (is_vertical)
                        {
                          x_pos = (x_step_boundaries[yind * 2] + x_step_boundaries[yind * 2 + 1]) / 2;
                          y_pos = y[yind];
                          childchild->setAttribute("x", y_pos);
                          childchild->setAttribute("y", x_pos);
                        }
                      else
                        {
                          x_pos = (x_step_boundaries[xind * 2] + x_step_boundaries[xind * 2 + 1]) / 2;
                          y_pos = y[xind];
                          childchild->setAttribute("x", x_pos);
                          childchild->setAttribute("y", y_pos);
                        }
                      global_render->setMarkerSize(childchild,
                                                   1.5 * (len / (is_vertical ? (ymax - ymin) : (xmax - xmin))));
                    }
                }
            }
        }
      else if (mkind == "all")
        {
          auto xind = static_cast<int>(element->getAttribute("xind"));
          auto yind = static_cast<int>(element->getAttribute("yind"));

          if (xind == -1 && yind == -1)
            {
              for (const auto &bar : child->children())
                {
                  for (const auto &rect : bar->children())
                    {
                      if (rect->hasAttribute("linecolorind")) continue;
                      rect->setAttribute("fillcolorind", 989);
                    }
                }
              continue;
            }

          bool is_horizontal = static_cast<std::string>(child->getAttribute("orientation")) == "horizontal";
          std::vector<std::shared_ptr<GRM::Element>> bar_groups = child->children();

          if ((is_horizontal && xind == -1) || (!is_horizontal && yind == -1))
            {
              continue;
            }
          if ((is_horizontal ? xind : yind) >= bar_groups.size())
            {
              continue;
            }

          int cnt = 0;
          for (const auto &group : bar_groups)
            {
              for (const auto &rect : group->children())
                {
                  if (rect->hasAttribute("linecolorind")) continue;
                  if (cnt == (is_horizontal ? xind : yind))
                    {
                      rect->setAttribute("fillcolorind", 2);
                    }
                  else
                    {
                      rect->setAttribute("fillcolorind", 989);
                    }
                  cnt += 1;
                }
            }
        }
    }
}

void GRM::Render::processLimits(const std::shared_ptr<GRM::Element> &element)
{
  /*!
   * processing function for gr_window
   *
   * \param[in] element The GRM::Element that contains the attributes
   */
  int adjust_xlim, adjust_ylim;
  std::string kind = static_cast<std::string>(element->getAttribute("kind"));
  int scale = 0;

  if (kind != "pie" && kind != "polar" && kind != "polar_histogram" && kind != "polar_heatmap" &&
      kind != "nonuniformpolar_heatmap")
    {
      scale |= static_cast<int>(element->getAttribute("xlog")) ? GR_OPTION_X_LOG : 0;
      scale |= static_cast<int>(element->getAttribute("ylog")) ? GR_OPTION_Y_LOG : 0;
      scale |= static_cast<int>(element->getAttribute("zlog")) ? GR_OPTION_Z_LOG : 0;
      scale |= static_cast<int>(element->getAttribute("xflip")) ? GR_OPTION_FLIP_X : 0;
      scale |= static_cast<int>(element->getAttribute("yflip")) ? GR_OPTION_FLIP_Y : 0;
      scale |= static_cast<int>(element->getAttribute("zflip")) ? GR_OPTION_FLIP_Z : 0;
    }
  element->setAttribute("scale", scale);

  double xmin = static_cast<double>(element->getAttribute("_xlim_min"));
  double xmax = static_cast<double>(element->getAttribute("_xlim_max"));
  double ymin = static_cast<double>(element->getAttribute("_ylim_min"));
  double ymax = static_cast<double>(element->getAttribute("_ylim_max"));

  if (element->hasAttribute("reset_ranges") && static_cast<int>(element->getAttribute("reset_ranges")))
    {
      if (element->hasAttribute("_original_xmin") && element->hasAttribute("_original_xmax") &&
          element->hasAttribute("_original_ymin") && element->hasAttribute("_original_ymax") &&
          element->hasAttribute("_original_adjust_xlim") && element->hasAttribute("_original_adjust_ylim"))
        {
          xmin = static_cast<double>(element->getAttribute("_original_xmin"));
          xmax = static_cast<double>(element->getAttribute("_original_xmax"));
          ymin = static_cast<double>(element->getAttribute("_original_ymin"));
          ymax = static_cast<double>(element->getAttribute("_original_ymax"));
          adjust_xlim = static_cast<int>(element->getAttribute("_original_adjust_xlim"));
          adjust_ylim = static_cast<int>(element->getAttribute("_original_adjust_ylim"));
          element->setAttribute("adjust_xlim", adjust_xlim);
          element->setAttribute("adjust_ylim", adjust_ylim);
          element->removeAttribute("_original_xlim");
          element->removeAttribute("_original_xmin");
          element->removeAttribute("_original_xmax");
          element->removeAttribute("_original_ylim");
          element->removeAttribute("_original_ymin");
          element->removeAttribute("_original_ymax");
          element->removeAttribute("_original_adjust_xlim");
          element->removeAttribute("_original_adjust_ylim");
        }
      element->removeAttribute("reset_ranges");
    }

  if (element->hasAttribute("panzoom") && static_cast<int>(element->getAttribute("panzoom")))
    {
      if (!element->hasAttribute("_original_xlim"))
        {
          element->setAttribute("_original_xmin", xmin);
          element->setAttribute("_original_xmax", xmax);
          element->setAttribute("_original_xlim", true);
          adjust_xlim = static_cast<int>(element->getAttribute("adjust_xlim"));
          element->setAttribute("_original_adjust_xlim", adjust_xlim);
          element->setAttribute("adjust_xlim", 0);
        }
      if (!element->hasAttribute("_original_ylim"))
        {
          element->setAttribute("_original_ymin", ymin);
          element->setAttribute("_original_ymax", ymax);
          element->setAttribute("_original_ylim", true);
          adjust_ylim = static_cast<int>(element->getAttribute("adjust_ylim"));
          element->setAttribute("_original_adjust_ylim", adjust_ylim);
          element->setAttribute("adjust_ylim", 0);
        }
      auto panzoom_element = element->getElementsByTagName("panzoom")[0];
      double x = static_cast<double>(panzoom_element->getAttribute("x"));
      double y = static_cast<double>(panzoom_element->getAttribute("y"));
      double xzoom = static_cast<double>(panzoom_element->getAttribute("xzoom"));
      double yzoom = static_cast<double>(panzoom_element->getAttribute("yzoom"));

      /* Ensure the correct window is set in GRM */
      bool window_exists = (element->hasAttribute("window_xmin") && element->hasAttribute("window_xmax") &&
                            element->hasAttribute("window_ymin") && element->hasAttribute("window_ymax"));
      if (window_exists)
        {
          double stored_window_xmin = static_cast<double>(element->getAttribute("window_xmin"));
          double stored_window_xmax = static_cast<double>(element->getAttribute("window_xmax"));
          double stored_window_ymin = static_cast<double>(element->getAttribute("window_ymin"));
          double stored_window_ymax = static_cast<double>(element->getAttribute("window_ymax"));

          gr_setwindow(stored_window_xmin, stored_window_xmax, stored_window_ymin, stored_window_ymax);
        }
      else
        {
          throw NotFoundError("Window not found\n");
        }

      gr_panzoom(x, y, xzoom, yzoom, &xmin, &xmax, &ymin, &ymax);

      element->removeAttribute("panzoom");
      element->removeChild(panzoom_element);

      for (const auto &child : element->children())
        {
          if (starts_with(child->localName(), "series_"))
            {
              resetOldBoundingBoxes(child);
            }
        }
    }

  element->setAttribute("_xlim_min", xmin);
  element->setAttribute("_xlim_max", xmax);
  element->setAttribute("_ylim_min", ymin);
  element->setAttribute("_ylim_max", ymax);

  if (!(scale & GR_OPTION_X_LOG))
    {
      adjust_xlim = static_cast<int>(element->getAttribute("adjust_xlim"));
      if (adjust_xlim)
        {
          logger((stderr, "_xlim before \"gr_adjustlimits\": (%lf, %lf)\n", xmin, xmax));
          gr_adjustlimits(&xmin, &xmax);
          logger((stderr, "_xlim after \"gr_adjustlimits\": (%lf, %lf)\n", xmin, xmax));
        }
    }

  if (!(scale & GR_OPTION_Y_LOG))
    {
      adjust_ylim = static_cast<int>(element->getAttribute("adjust_ylim"));
      if (adjust_ylim)
        {
          logger((stderr, "_ylim before \"gr_adjustlimits\": (%lf, %lf)\n", ymin, ymax));
          gr_adjustlimits(&ymin, &ymax);
          logger((stderr, "_ylim after \"gr_adjustlimits\": (%lf, %lf)\n", ymin, ymax));
        }
    }

  if (str_equals_any(kind.c_str(), 7, "wireframe", "surface", "plot3", "scatter3", "trisurf", "volume", "isosurface"))
    {
      double zmin = static_cast<double>(element->getAttribute("_zlim_min"));
      double zmax = static_cast<double>(element->getAttribute("_zlim_max"));
      if (zmax > 0)
        {
          if (!(scale & GR_OPTION_Z_LOG))
            {
              int adjust_zlim = static_cast<int>(element->hasAttribute("adjust_zlim"));
              if (adjust_zlim)
                {
                  logger((stderr, "_zlim before \"gr_adjustlimits\": (%lf, %lf)\n", zmin, zmax));
                  gr_adjustlimits(&zmin, &zmax);
                  logger((stderr, "_zlim after \"gr_adjustlimits\": (%lf, %lf)\n", zmin, zmax));
                }
            }
          logger((stderr, "Storing window3d (%lf, %lf, %lf, %lf, %lf, %lf)\n", xmin, xmax, ymin, ymax, zmin, zmax));
          global_render->setWindow3d(element, xmin, xmax, ymin, ymax, zmin, zmax);
        }
    }
  else
    {
      logger((stderr, "Storing window (%lf, %lf, %lf, %lf)\n", xmin, xmax, ymin, ymax));
      // todo: grplot zoom etc. dont set -1 1 -1 1 window here --> use _xlim _ylim like in plot.cxx
      global_render->setWindow(element, xmin, xmax, ymin, ymax);
    }
  processScale(element);
}

static void processLineColorInd(const std::shared_ptr<GRM::Element> &element)
{
  gr_setlinecolorind(static_cast<int>(element->getAttribute("linecolorind")));
}

static void processLineSpec(const std::shared_ptr<GRM::Element> &element)
{
  gr_uselinespec((static_cast<std::string>(element->getAttribute("linespec"))).data());
}

static void processLineType(const std::shared_ptr<GRM::Element> &element)
{
  gr_setlinetype(static_cast<int>(element->getAttribute("linetype")));
}

static void processLineWidth(const std::shared_ptr<GRM::Element> &element)
{
  gr_setlinewidth(static_cast<double>(element->getAttribute("linewidth")));
}

static void processMarkerColorInd(const std::shared_ptr<GRM::Element> &element)
{
  gr_setmarkercolorind(static_cast<int>(element->getAttribute("markercolorind")));
}

static void processMarkerSize(const std::shared_ptr<GRM::Element> &element)
{
  gr_setmarkersize(static_cast<double>(element->getAttribute("markersize")));
}

static void processMarkerType(const std::shared_ptr<GRM::Element> &element)
{
  gr_setmarkertype(static_cast<int>(element->getAttribute("markertype")));
}

static void processProjectionType(const std::shared_ptr<GRM::Element> &element)
{
  gr_setprojectiontype(static_cast<int>(element->getAttribute("projectiontype")));
}

static void processRelativeCharHeight(const std::shared_ptr<GRM::Element> &element)
{
  double viewport[4];
  auto subplot_element = getSubplotElement(element);
  double charheight, diagFactor, maxCharHeight;

  if (!subplot_element->hasAttribute("viewport_xmin") || !subplot_element->hasAttribute("viewport_xmax") ||
      !subplot_element->hasAttribute("viewport_ymin") || !subplot_element->hasAttribute("viewport_ymax"))
    {
      throw NotFoundError("Viewport not found\n");
    }
  viewport[0] = static_cast<double>(subplot_element->getAttribute("viewport_xmin"));
  viewport[1] = static_cast<double>(subplot_element->getAttribute("viewport_xmax"));
  viewport[2] = static_cast<double>(subplot_element->getAttribute("viewport_ymin"));
  viewport[3] = static_cast<double>(subplot_element->getAttribute("viewport_ymax"));
  diagFactor = static_cast<double>(element->getAttribute("diag_factor"));
  maxCharHeight = static_cast<double>(element->getAttribute("max_charheight"));

  double diag = std::sqrt((viewport[1] - viewport[0]) * (viewport[1] - viewport[0]) +
                          (viewport[3] - viewport[2]) * (viewport[3] - viewport[2]));

  charheight = std::max<double>(diag * diagFactor, maxCharHeight);
  gr_setcharheight(charheight);
}

static void processResampleMethod(const std::shared_ptr<GRM::Element> &element)
{
  unsigned int resample_method_flag;
  if (!element->getAttribute("resample_method").isInt())
    {
      auto resample_method_str = static_cast<std::string>(element->getAttribute("resample_method"));

      if (resample_method_str == "nearest")
        {
          resample_method_flag = GKS_K_RESAMPLE_NEAREST;
        }
      else if (resample_method_str == "linear")
        {
          resample_method_flag = GKS_K_RESAMPLE_LINEAR;
        }
      else if (resample_method_str == "lanczos")
        {
          resample_method_flag = GKS_K_RESAMPLE_LANCZOS;
        }
      else
        {
          resample_method_flag = GKS_K_RESAMPLE_DEFAULT;
        }
    }
  else
    {
      resample_method_flag = static_cast<int>(element->getAttribute("resample_method"));
    }
  gr_setresamplemethod(resample_method_flag);
}

void GRM::Render::processScale(const std::shared_ptr<GRM::Element> &element)
{
  gr_setscale(static_cast<int>(element->getAttribute("scale")));
}

static void processSelntran(const std::shared_ptr<GRM::Element> &element)
{
  gr_selntran(static_cast<int>(element->getAttribute("selntran")));
}

static void processSpace(const std::shared_ptr<GRM::Element> &element)
{
  double zmin, zmax;
  int rotation, tilt;
  zmin = (double)element->getAttribute("space_zmin");
  zmax = (double)element->getAttribute("space_zmax");
  rotation = (int)element->getAttribute("space_rotation");
  tilt = (int)element->getAttribute("space_tilt");

  gr_setspace(zmin, zmax, rotation, tilt);
}

static void processSpace3d(const std::shared_ptr<GRM::Element> &element)
{
  double phi = PLOT_DEFAULT_ROTATION, theta = PLOT_DEFAULT_TILT, fov, camera_distance;

  if (element->hasAttribute("space3d_phi"))
    {
      phi = (double)element->getAttribute("space3d_phi");
    }
  else
    {
      element->setAttribute("space3d_phi", phi);
    }
  if (element->hasAttribute("space3d_theta"))
    {
      theta = (double)element->getAttribute("space3d_theta");
    }
  else
    {
      element->setAttribute("space3d_theta", theta);
    }
  fov = (double)element->getAttribute("space3d_fov");
  camera_distance = (double)element->getAttribute("space3d_camera_distance");

  gr_setspace3d(-phi, theta, fov, camera_distance);
}

static void processSubplot(const std::shared_ptr<GRM::Element> &element)
{
  /*!
   * processing function for the subplot
   *
   * \param[in] element The GRM::Element that contains the attributes
   */
  int y_label_margin, x_label_margin, title_margin;
  std::string kind;
  double subplot[4];
  int keep_aspect_ratio;
  double metric_width, metric_height;
  int pixel_width, pixel_height;
  double aspect_ratio_ws;
  double vp[4];
  double vp0, vp1, vp2, vp3;
  double left_margin, right_margin, bottom_margin, top_margin;
  double viewport[4] = {0.0, 0.0, 0.0, 0.0};
  int background_color_index;

  subplot[0] = (double)element->getAttribute("plot_xmin");
  subplot[1] = (double)element->getAttribute("plot_xmax");
  subplot[2] = (double)element->getAttribute("plot_ymin");
  subplot[3] = (double)element->getAttribute("plot_ymax");
  kind = (std::string)element->getAttribute("kind");
  y_label_margin = (int)element->getAttribute("ylabel_margin");
  x_label_margin = (int)element->getAttribute("xlabel_margin");
  title_margin = (int)element->getAttribute("title_margin");
  keep_aspect_ratio = static_cast<int>(element->getAttribute("keep_aspect_ratio"));

  GRM::Render::get_figure_size(&pixel_width, &pixel_height, nullptr, nullptr);

  aspect_ratio_ws = (double)pixel_width / pixel_height;
  memcpy(vp, subplot, sizeof(vp));
  if (aspect_ratio_ws > 1)
    {
      vp[2] /= aspect_ratio_ws;
      vp[3] /= aspect_ratio_ws;
      if (keep_aspect_ratio)
        {
          double border = 0.5 * (vp[1] - vp[0]) * (1.0 - 1.0 / aspect_ratio_ws);
          vp[0] += border;
          vp[1] -= border;
        }
    }
  else
    {
      vp[0] *= aspect_ratio_ws;
      vp[1] *= aspect_ratio_ws;
      if (keep_aspect_ratio)
        {
          double border = 0.5 * (vp[3] - vp[2]) * (1.0 - aspect_ratio_ws);
          vp[2] += border;
          vp[3] -= border;
        }
    }

  if (str_equals_any(kind.c_str(), 6, "wireframe", "surface", "plot3", "scatter3", "trisurf", "volume"))
    {
      double extent;

      extent = grm_min(vp[1] - vp[0], vp[3] - vp[2]);
      vp0 = 0.5 * (vp[0] + vp[1] - extent);
      vp1 = 0.5 * (vp[0] + vp[1] + extent);
      vp2 = 0.5 * (vp[2] + vp[3] - extent);
      vp3 = 0.5 * (vp[2] + vp[3] + extent);
    }
  else
    {
      vp0 = vp[0];
      vp1 = vp[1];
      vp2 = vp[2];
      vp3 = vp[3];
    }

  left_margin = y_label_margin ? 0.05 : 0;
  if (str_equals_any(kind.c_str(), 13, "contour", "contourf", "hexbin", "heatmap", "nonuniformheatmap", "surface",
                     "tricont", "trisurf", "volume", "marginalheatmap", "quiver", "polar_heatmap",
                     "nonuniformpolar_heatmap"))
    {
      right_margin = (vp1 - vp0) * 0.1;
    }
  else
    {
      right_margin = 0;
    }
  bottom_margin = x_label_margin ? 0.05 : 0;

  if (kind == "marginalheatmap")
    {
      top_margin = title_margin ? 0.075 + 0.5 * (vp[1] - vp[0]) * (1.0 - 1.0 / aspect_ratio_ws)
                                : 0.5 * (vp[1] - vp[0]) * (1.0 - 1.0 / aspect_ratio_ws);
      if (keep_aspect_ratio && !str_equals_any(kind.c_str(), 2, "surface", "volume"))
        right_margin += title_margin ? 0.075 : 0;
    }
  else
    {
      top_margin = title_margin ? 0.075 : 0;
      if (keep_aspect_ratio && !str_equals_any(kind.c_str(), 2, "surface", "volume"))
        right_margin -= 0.5 * (vp[1] - vp[0]) * (1.0 - 1.0 / aspect_ratio_ws) - top_margin;
    }
  if (kind == "imshow")
    {
      unsigned int i;
      unsigned int *shape;
      double w, h, x_min, x_max, y_min, y_max, *x, *y;

      int cols = static_cast<int>(element->getAttribute("cols"));
      int rows = static_cast<int>(element->getAttribute("rows"));

      h = (double)rows / (double)cols * (vp[1] - vp[0]);
      w = (double)cols / (double)rows * (vp[3] - vp[2]);

      x_min = grm_max(0.5 * (vp[0] + vp[1] - w), vp[0]);
      x_max = grm_min(0.5 * (vp[0] + vp[1] + w), vp[1]);
      y_min = grm_max(0.5 * (vp[3] + vp[2] - h), vp[2]);
      y_max = grm_min(0.5 * (vp[3] + vp[2] + h), vp[3]);

      left_margin = (x_min == vp[0]) ? -0.075 : (x_min - vp[0]) / (vp[1] - vp[0]) - 0.075;
      right_margin = (x_max == vp[1]) ? -0.05 : 0.95 - (x_max - vp[0]) / (vp[1] - vp[0]);
      bottom_margin = (y_min == vp[2]) ? -0.075 : (y_min - vp[2]) / (vp[3] - vp[2]) - 0.075;
      top_margin = (y_max == vp[3]) ? -0.025 : 0.975 - (y_max - vp[2]) / (vp[3] - vp[2]);
    }

  viewport[0] = vp0 + (0.075 + left_margin) * (vp1 - vp0);
  viewport[1] = vp0 + (0.95 - right_margin) * (vp1 - vp0);
  viewport[2] = vp2 + (0.075 + bottom_margin) * (vp3 - vp2);
  viewport[3] = vp2 + (0.975 - top_margin) * (vp3 - vp2);

  if (str_equals_any(kind.c_str(), 4, "line", "stairs", "scatter", "stem"))
    {
      double w, h;
      int location = PLOT_DEFAULT_LOCATION;
      if (element->hasAttribute("location"))
        {
          location = static_cast<int>(element->getAttribute("location"));
        }
      else
        {
          element->setAttribute("location", location);
        }

      if (location == 11 || location == 12 || location == 13)
        {
          legend_size(element, &w, &h);
          viewport[1] -= w + 0.1;
        }
    }

  if (element->hasAttribute("backgroundcolor"))
    {
      background_color_index = static_cast<int>(element->getAttribute("backgroundcolor"));
      gr_savestate();
      gr_selntran(0);
      gr_setfillintstyle(GKS_K_INTSTYLE_SOLID);
      gr_setfillcolorind(background_color_index);
      if (aspect_ratio_ws > 1)
        {
          if (redrawws) gr_fillrect(subplot[0], subplot[1], subplot[2] / aspect_ratio_ws, subplot[3] / aspect_ratio_ws);
        }
      else
        {
          if (redrawws) gr_fillrect(subplot[0] * aspect_ratio_ws, subplot[1] * aspect_ratio_ws, subplot[2], subplot[3]);
        }
      gr_selntran(1);
      gr_restorestate();
    }

  if (str_equals_any(kind.c_str(), 5, "pie", "polar", "polar_histogram", "polar_heatmap", "nonuniformpolar_heatmap"))
    {
      double x_center, y_center, r;

      x_center = 0.5 * (viewport[0] + viewport[1]);
      y_center = 0.5 * (viewport[2] + viewport[3]);
      r = 0.45 * grm_min(viewport[1] - viewport[0], viewport[3] - viewport[2]);
      if (title_margin)
        {
          r *= 0.975;
          y_center -= 0.025 * r;
        }
      viewport[0] = x_center - r;
      viewport[1] = x_center + r;
      viewport[2] = y_center - r;
      viewport[3] = y_center + r;
    }

  gr_setviewport(viewport[0], viewport[1], viewport[2], viewport[3]);
  element->setAttribute("viewport_xmin", viewport[0]);
  element->setAttribute("viewport_xmax", viewport[1]);
  element->setAttribute("viewport_ymin", viewport[2]);
  element->setAttribute("viewport_ymax", viewport[3]);
  element->setAttribute("vp_xmin", vp[0]);
  element->setAttribute("vp_xmax", vp[1]);
  element->setAttribute("vp_ymin", vp[2]);
  element->setAttribute("vp_ymax", vp[3]);
}

static void processTextAlign(const std::shared_ptr<GRM::Element> &element)
{
  gr_settextalign(static_cast<int>(element->getAttribute("textalign_horizontal")),
                  static_cast<int>(element->getAttribute("textalign_vertical")));
}

static void processTextColorForBackground(const std::shared_ptr<GRM::Element> &element)
/*  The set_text_color_for_background function used in plot.cxx now as an attribute function
    It is now possible to inquire colors during runtime -> No colors are given as parameters
    The new color is set on `element`
    There are no params apart from element
    \param[in] element The GRM::Element the color should be set in. Also contains other attributes which may function as
 parameters

    Attributes as Parameters (with prefix "stcfb-"):
    plot: for which plot it is used: right now only pie plot
 */
{
  int color_ind;
  int inq_color;
  unsigned char color_rgb[4];
  std::string plot = "pie";

  if (element->hasAttribute("stcfb_plot"))
    {
      plot = static_cast<std::string>(element->getAttribute("stcfb_plot"));
    }

  if (plot == "pie")
    {
      double r, g, b;
      double color_lightness;
      std::shared_ptr<GRM::Render> render;

      render = std::dynamic_pointer_cast<GRM::Render>(element->ownerDocument());
      if (!render)
        {
          throw NotFoundError("Render-document not found for element\n");
        }
      if (element->hasAttribute("color_index"))
        {
          color_ind = static_cast<int>(element->getAttribute("color_index"));
        }
      else
        {
          gr_inqfillcolorind(&color_ind);
        }
      gr_inqcolor(color_ind, (int *)color_rgb);

      r = color_rgb[0] / 255.0;
      g = color_rgb[1] / 255.0;
      b = color_rgb[2] / 255.0;

      color_lightness = get_lightness_from_rbg(r, g, b);
      if (color_lightness < 0.4)
        {
          gr_settextcolorind(0);
        }
      else
        {
          gr_settextcolorind(1);
        }
    }
}

static void processTextColorInd(const std::shared_ptr<GRM::Element> &element)
{

  gr_settextcolorind(static_cast<int>(element->getAttribute("textcolorind")));
}

static void processTextEncoding(const std::shared_ptr<GRM::Element> &element)
{

  gr_settextencoding(static_cast<int>(element->getAttribute("textencoding")));
}

static void processTextFontPrec(const std::shared_ptr<GRM::Element> &element)
{

  gr_settextfontprec(static_cast<int>(element->getAttribute("textfontprec_font")),
                     static_cast<int>(element->getAttribute("textfontprec_prec")));
}

static void processTextPath(const std::shared_ptr<GRM::Element> &element)
{
  gr_settextpath(static_cast<int>(element->getAttribute("textpath")));
}

static void processTitle(const std::shared_ptr<GRM::Element> &element)
{
  double viewport[4], vp[4];
  del_values del = del_values::update_without_default;

  auto subplot_element = getSubplotElement(element);
  std::string name = (std::string)subplot_element->getAttribute("kind");
  viewport[0] = (double)subplot_element->getAttribute("viewport_xmin");
  viewport[1] = (double)subplot_element->getAttribute("viewport_xmax");
  viewport[2] = (double)subplot_element->getAttribute("viewport_ymin");
  viewport[3] = (double)subplot_element->getAttribute("viewport_ymax");
  vp[0] = (double)subplot_element->getAttribute("vp_xmin");
  vp[1] = (double)subplot_element->getAttribute("vp_xmax");
  vp[2] = (double)subplot_element->getAttribute("vp_ymin");
  vp[3] = (double)subplot_element->getAttribute("vp_ymax");

  double x = 0.5 * (viewport[0] + viewport[1]);
  double y = vp[3];
  std::string title = static_cast<std::string>(element->getAttribute("title"));

  if (title.empty()) return; // Empty title is pointless, no need to waste the space for nothing
  if (auto render = std::dynamic_pointer_cast<GRM::Render>(element->ownerDocument()))
    {
      // title is unique so child_id isn't needed here
      del = del_values(static_cast<int>(element->getAttribute("_delete_children")));
      auto title_elem = element->querySelectors("[name=\"title\"]");
      if ((del != del_values::update_without_default && del != del_values::update_with_default) ||
          title_elem == nullptr)
        {
          if (title_elem != nullptr) title_elem->remove();
          title_elem = render->createText(x, y, title);
          title_elem->setAttribute("name", "title");
          render->setTextAlign(title_elem, GKS_K_TEXT_HALIGN_CENTER, GKS_K_TEXT_VALIGN_TOP);
          subplot_element->append(title_elem);
        }
      else if (title_elem != nullptr)
        {
          title_elem = render->createText(x, y, title, CoordinateSpace::NDC, title_elem);
        }
    }
}

static void processTransparency(const std::shared_ptr<GRM::Element> &element)
{
  gr_settransparency(static_cast<double>(element->getAttribute("transparency")));
}

void GRM::Render::processWindow(const std::shared_ptr<GRM::Element> &element)
{
  auto kind = static_cast<std::string>(element->getAttribute("kind"));

  double xmin = static_cast<double>(element->getAttribute("window_xmin"));
  double xmax = static_cast<double>(element->getAttribute("window_xmax"));
  double ymin = static_cast<double>(element->getAttribute("window_ymin"));
  double ymax = static_cast<double>(element->getAttribute("window_ymax"));

  if (str_equals_any(kind.c_str(), 4, "polar", "polar_histogram", "polar_heatmap", "nonuniformpolar_heatmap"))
    {
      gr_setwindow(-1, 1, -1, 1);
    }
  else
    {
      gr_setwindow(xmin, xmax, ymin, ymax);
    }
  if (str_equals_any(kind.c_str(), 7, "wireframe", "surface", "plot3", "scatter3", "trisurf", "volume", "isosurface"))
    {
      double zmin = static_cast<double>(element->getAttribute("window_zmin"));
      double zmax = static_cast<double>(element->getAttribute("window_zmax"));

      gr_setwindow3d(xmin, xmax, ymin, ymax, zmin, zmax);
    }
}

static void processWSViewport(const std::shared_ptr<GRM::Element> &element)
{
  /*!
   * processing function for gr_wsviewport
   *
   * \param[in] element The GRM::Element that contains the attributes
   */
  double wsviewport[4];
  wsviewport[0] = (double)element->getAttribute("wsviewport_xmin");
  wsviewport[1] = (double)element->getAttribute("wsviewport_xmax");
  wsviewport[2] = (double)element->getAttribute("wsviewport_ymin");
  wsviewport[3] = (double)element->getAttribute("wsviewport_ymax");

  gr_setwsviewport(wsviewport[0], wsviewport[1], wsviewport[2], wsviewport[3]);
}

static void processWSWindow(const std::shared_ptr<GRM::Element> &element)
{
  double wswindow[4];
  double xmin = static_cast<double>(element->getAttribute("wswindow_xmin"));
  double xmax = static_cast<double>(element->getAttribute("wswindow_xmax"));
  double ymin = static_cast<double>(element->getAttribute("wswindow_ymin"));
  double ymax = static_cast<double>(element->getAttribute("wswindow_ymax"));

  gr_setwswindow(xmin, xmax, ymin, ymax);
}

void GRM::Render::processViewport(const std::shared_ptr<GRM::Element> &element)
{
  /*!
   * processing function for gr_viewport
   *
   * \param[in] element The GRM::Element that contains the attributes
   */
  double viewport[4];
  double diag;
  double charheight;
  std::string kind;

  viewport[0] = (double)element->getAttribute("viewport_xmin");
  viewport[1] = (double)element->getAttribute("viewport_xmax");
  viewport[2] = (double)element->getAttribute("viewport_ymin");
  viewport[3] = (double)element->getAttribute("viewport_ymax");
  kind = (std::string)element->getAttribute("kind");

  diag = std::sqrt((viewport[1] - viewport[0]) * (viewport[1] - viewport[0]) +
                   (viewport[3] - viewport[2]) * (viewport[3] - viewport[2]));

  if (str_equals_any(kind.c_str(), 6, "wireframe", "surface", "plot3", "scatter3", "trisurf", "volume"))
    {
      charheight = grm_max(0.024 * diag, 0.012);
    }
  else
    {
      charheight = grm_max(0.018 * diag, 0.012);
    }
  element->setAttribute("charheight", charheight);
}

static void processXlabel(const std::shared_ptr<GRM::Element> &element)
{
  double viewport[4], vp[4], charheight;
  del_values del = del_values::update_without_default;

  auto subplot_element = getSubplotElement(element);

  gr_inqcharheight(&charheight);
  viewport[0] = (double)subplot_element->getAttribute("viewport_xmin");
  viewport[1] = (double)subplot_element->getAttribute("viewport_xmax");
  viewport[2] = (double)subplot_element->getAttribute("viewport_ymin");
  viewport[3] = (double)subplot_element->getAttribute("viewport_ymax");
  vp[0] = (double)subplot_element->getAttribute("vp_xmin");
  vp[1] = (double)subplot_element->getAttribute("vp_xmax");
  vp[2] = (double)subplot_element->getAttribute("vp_ymin");
  vp[3] = (double)subplot_element->getAttribute("vp_ymax");

  double x = 0.5 * (viewport[0] + viewport[1]);
  double y = vp[2] + 0.5 * charheight;
  std::string x_label = (std::string)element->getAttribute("xlabel");
  if (x_label.empty()) return; // Empty xlabel is pointless, no need to waste the space for nothing

  if (auto render = std::dynamic_pointer_cast<GRM::Render>(element->ownerDocument()))
    {
      del = del_values(static_cast<int>(element->getAttribute("_delete_children")));
      auto xlabel_elem = element->querySelectors("[name=\"xlabel\"]");

      if ((del != del_values::update_without_default && del != del_values::update_with_default) ||
          xlabel_elem == nullptr)
        {
          if (xlabel_elem != nullptr) xlabel_elem->remove();
          xlabel_elem = render->createText(x, y, x_label);
          render->setTextAlign(xlabel_elem, GKS_K_TEXT_HALIGN_CENTER, GKS_K_TEXT_VALIGN_BOTTOM);
          element->appendChild(xlabel_elem);
        }
      else if (xlabel_elem != nullptr)
        {
          render->createText(x, y, x_label, CoordinateSpace::NDC, xlabel_elem);
        }
      if (xlabel_elem != nullptr) xlabel_elem->setAttribute("name", "xlabel");
    }
}

static void processYlabel(const std::shared_ptr<GRM::Element> &element)
{
  double viewport[4], vp[4], charheight;
  int keep_aspect_ratio;
  del_values del = del_values::update_without_default;

  auto subplot_element = getSubplotElement(element);

  gr_inqcharheight(&charheight);
  viewport[0] = (double)subplot_element->getAttribute("viewport_xmin");
  viewport[1] = (double)subplot_element->getAttribute("viewport_xmax");
  viewport[2] = (double)subplot_element->getAttribute("viewport_ymin");
  viewport[3] = (double)subplot_element->getAttribute("viewport_ymax");
  vp[0] = (double)subplot_element->getAttribute("vp_xmin");
  vp[1] = (double)subplot_element->getAttribute("vp_xmax");
  vp[2] = (double)subplot_element->getAttribute("vp_ymin");
  vp[3] = (double)subplot_element->getAttribute("vp_ymax");
  keep_aspect_ratio = (int)subplot_element->getAttribute("keep_aspect_ratio");

  double x = ((keep_aspect_ratio) ? 0.925 : 1) * vp[0] + 0.5 * charheight;
  double y = 0.5 * (viewport[2] + viewport[3]);
  std::string y_label = (std::string)element->getAttribute("ylabel");
  if (y_label.empty()) return; // Empty ylabel is pointless, no need to waste the space for nothing

  if (auto render = std::dynamic_pointer_cast<GRM::Render>(element->ownerDocument()))
    {
      del = del_values(static_cast<int>(element->getAttribute("_delete_children")));
      auto ylabel_elem = element->querySelectors("[name=\"ylabel\"]");

      if ((del != del_values::update_without_default && del != del_values::update_with_default) ||
          ylabel_elem == nullptr)
        {
          if (ylabel_elem != nullptr) ylabel_elem->remove();
          ylabel_elem = render->createText(x, y, y_label);
          render->setTextAlign(ylabel_elem, GKS_K_TEXT_HALIGN_CENTER, GKS_K_TEXT_VALIGN_TOP);
          render->setCharUp(ylabel_elem, -1, 0);
          element->appendChild(ylabel_elem);
        }
      else if (ylabel_elem != nullptr)
        {
          render->createText(x, y, y_label, CoordinateSpace::NDC, ylabel_elem);
        }
      if (ylabel_elem != nullptr) ylabel_elem->setAttribute("name", "ylabel");
    }
}

static void processXTickLabels(const std::shared_ptr<GRM::Element> &element)
{
  double viewport[4], charheight;
  std::vector<std::string> xticklabels;
  del_values del = del_values::update_without_default;
  int child_id = 0;
  bool init = false;

  auto subplot_element = getSubplotElement(element);

  gr_inqcharheight(&charheight);
  viewport[0] = (double)subplot_element->getAttribute("viewport_xmin");
  viewport[1] = (double)subplot_element->getAttribute("viewport_xmax");
  viewport[2] = (double)subplot_element->getAttribute("viewport_ymin");
  viewport[3] = (double)subplot_element->getAttribute("viewport_ymax");

  if (auto render = std::dynamic_pointer_cast<GRM::Render>(element->ownerDocument()))
    {
      std::shared_ptr<GRM::Element> xtick_element;
      std::shared_ptr<GRM::Context> context = render->getContext();
      std::string key = static_cast<std::string>(element->getAttribute("xticklabels"));
      xticklabels = GRM::get<std::vector<std::string>>((*context)[key]);

      double x, y;
      double x_left = 0, x_right = 1, null;
      double available_width;
      xtick_element = element->querySelectors("xticklabel_group");
      if (xtick_element == nullptr)
        {
          xtick_element = render->createElement("xticklabel_group");
          render->setTextAlign(xtick_element, GKS_K_TEXT_HALIGN_CENTER, GKS_K_TEXT_VALIGN_TOP);
          xtick_element->setAttribute("name", "xticklabels");
          element->append(xtick_element);
        }

      del = del_values(static_cast<int>(xtick_element->getAttribute("_delete_children")));
      if (del != del_values::update_without_default)
        {
          for (const auto &child : xtick_element->children())
            {
              if (del == del_values::recreate_own_children)
                {
                  if (child->hasAttribute("_child_id")) child->remove();
                }
              else if (del == del_values::recreate_all_children)
                {
                  child->remove();
                }
            }
        }
      else if (!xtick_element->hasChildNodes())
        init = true;

      int offset = 1;
      std::string kind = static_cast<std::string>(subplot_element->getAttribute("kind"));

      /* calculate width available for xticknotations */
      gr_wctondc(&x_left, &null);
      gr_wctondc(&x_right, &null);
      available_width = x_right - x_left;
      if (str_equals_any(kind.c_str(), 4, "barplot", "hist", "stem", "stairs")) offset = 0;
      for (int i = 1; i <= xticklabels.size(); i++)
        {
          x = i - offset;
          gr_wctondc(&x, &y);
          y = viewport[2] - 0.5 * charheight;
          draw_xticklabel(x, y, xticklabels[i - 1].c_str(), available_width, xtick_element, init, child_id++);
        }
    }
}

static void processYTickLabels(const std::shared_ptr<GRM::Element> &element)
{
  double viewport[4], charheight;
  std::vector<std::string> yticklabels;
  del_values del = del_values::update_without_default;
  int child_id = 0;
  bool init = false;

  auto subplot_element = getSubplotElement(element);

  gr_inqcharheight(&charheight);
  viewport[0] = (double)subplot_element->getAttribute("viewport_xmin");
  viewport[1] = (double)subplot_element->getAttribute("viewport_xmax");
  viewport[2] = (double)subplot_element->getAttribute("viewport_ymin");
  viewport[3] = (double)subplot_element->getAttribute("viewport_ymax");

  if (auto render = std::dynamic_pointer_cast<GRM::Render>(element->ownerDocument()))
    {
      std::shared_ptr<GRM::Element> ytick_element;
      std::shared_ptr<GRM::Context> context = render->getContext();
      std::string key = static_cast<std::string>(element->getAttribute("yticklabels"));
      yticklabels = GRM::get<std::vector<std::string>>((*context)[key]);

      double x, y;
      double y_left = 0, y_right = 1, null;
      double available_height;
      ytick_element = element->querySelectors("yticklabel_group");
      if (ytick_element == nullptr)
        {
          ytick_element = render->createElement("yticklabel_group");
          render->setTextAlign(ytick_element, GKS_K_TEXT_HALIGN_CENTER, GKS_K_TEXT_VALIGN_BOTTOM);
          ytick_element->setAttribute("name", "yticklabels");
          element->append(ytick_element);
        }

      del = del_values(static_cast<int>(ytick_element->getAttribute("_delete_children")));
      if (del != del_values::update_without_default)
        {
          for (const auto &child : ytick_element->children())
            {
              if (del == del_values::recreate_own_children)
                {
                  if (child->hasAttribute("_child_id")) child->remove();
                }
              else if (del == del_values::recreate_all_children)
                {
                  child->remove();
                }
            }
        }
      else if (!ytick_element->hasChildNodes())
        init = true;

      /* calculate width available for yticknotations */
      gr_wctondc(&null, &y_left);
      gr_wctondc(&null, &y_right);
      available_height = y_right - y_left;
      for (int i = 1; i <= yticklabels.size(); i++)
        {
          y = i;
          gr_wctondc(&x, &y);
          y -= 0.55 * charheight;
          x = viewport[0] - 1.5 * charheight; // correct would be charwidth here
          draw_yticklabel(x, y, yticklabels[i - 1].c_str(), available_height, ytick_element, init, child_id++);
        }
    }
}

static void processZIndex(const std::shared_ptr<GRM::Element> &element)
{
  if (!zQueueIsBeingRendered)
    {
      int zIndex = static_cast<int>(element->getAttribute("z_index"));
      zIndexManager.setZIndex(zIndex);
    }
}

void GRM::Render::processAttributes(const std::shared_ptr<GRM::Element> &element)
{
  /*!
   * processing function for all kinds of attributes
   *
   * \param[in] element The GRM::Element containing attributes
   */

  //! Map used for processing all kinds of attributes
  static std::map<std::string, std::function<void(const std::shared_ptr<GRM::Element> &)>> attrStringToFunc{
      {std::string("alpha"), processAlpha},
      {std::string("bordercolorind"), processBorderColorInd},
      {std::string("calc_window_and_viewport_from_parent"), processCalcWindowAndViewportFromParent},
      {std::string("charexpan"), processCharExpan},
      {std::string("charspace"), processCharSpace},
      {std::string("charup_x"), processCharUp}, // the x element can be used cause both must be set
      {std::string("clipxform"), processClipXForm},
      {std::string("offset"), processColorbarPosition},
      {std::string("colormap"), processColormap},
      {std::string("fillcolorind"), processFillColorInd},
      {std::string("fillintstyle"), processFillIntStyle},
      {std::string("fillstyle"), processFillStyle},
      {std::string("xflip"), processFlip}, // yflip is also set
      {std::string("font"), processFont},
      {std::string("gr_option_flip_x"), processGROptionFlipX},
      {std::string("gr_option_flip_y"), processGROptionFlipY},
      {std::string("gr3backgroundcolor_red"), processGR3BackgroundColor}, // all other colors must be set too
      {std::string("gr3cameralookat"), processGR3CameraLookAt},
      {std::string("linecolorind"), processLineColorInd},
      {std::string("linespec"), processLineSpec},
      {std::string("linetype"), processLineType},
      {std::string("linewidth"), processLineWidth},
      {std::string("marginalheatmap_kind"), processMarginalheatmapKind},
      {std::string("markercolorind"), processMarkerColorInd},
      {std::string("markersize"), processMarkerSize},
      {std::string("markertype"), processMarkerType},
      {std::string("projectiontype"), processProjectionType},
      {std::string("max_charheight"), processRelativeCharHeight},
      {std::string("resample_method"), processResampleMethod},
      {std::string("selntran"), processSelntran},
      {std::string("space"), processSpace},
      {std::string("space3d_fov"), processSpace3d},          // the fov element can be used cause both must be set
      {std::string("textalign_vertical"), processTextAlign}, // the alignment in both directions is set
      {std::string("textcolorind"), processTextColorInd},
      {std::string("textencoding"), processTextEncoding},
      {std::string("textfontprec_font"), processTextFontPrec}, // the font element can be used cause both must be set
      {std::string("textpath"), processTextPath},
      {std::string("title"), processTitle},
      {std::string("wsviewport_xmin"), processWSViewport}, // the xmin element can be used here cause all 4 are required
      {std::string("wswindow_xmin"), processWSWindow},     // the xmin element can be used here cause all 4 are required
      {std::string("xlabel"), processXlabel},
      {std::string("xticklabels"), processXTickLabels},
      {std::string("ylabel"), processYlabel},
      {std::string("yticklabels"), processYTickLabels},
      {std::string("z_index"), processZIndex},
  };

  static std::map<std::string, std::function<void(const std::shared_ptr<GRM::Element> &)>> attrStringToFuncPost{
      /* This map contains functions for attributes that should be called after some attributes have been processed
       * already. These functions can contain e.g. inquire function calls for colors.
       * */
      {std::string("transparency"), processTransparency},
      {std::string("set_text_color_for_background"), processTextColorForBackground},
  };

  static std::map<std::string, std::function<void(const std::shared_ptr<GRM::Element> &, const std::string attribute)>>
      multiAttrStringToFunc{
          /* This map contains functions for attributes of which an element can hold more than one e.g. colorrep
           * */
          {std::string("colorrep"), processColorRep},
      };

  for (const auto &attribute : element->getAttributeNames())
    {
      auto start = 0U;
      auto end = attribute.find('.');
      if (end != std::string::npos)
        {
          /* element can hold more than one attribute of this kind */
          auto attributeKind = attribute.substr(start, end);
          if (multiAttrStringToFunc.find(attributeKind) != multiAttrStringToFunc.end())
            {
              multiAttrStringToFunc[attributeKind](element, attribute);
            }
        }
      else if (attrStringToFunc.find(attribute) != attrStringToFunc.end())
        {
          attrStringToFunc[attribute](element);
        }
    }

  for (auto &attribute : element->getAttributeNames())
    /*
     * Post process attribute run
     */
    {
      if (attrStringToFuncPost.find(attribute) != attrStringToFuncPost.end())
        {
          attrStringToFuncPost[attribute](element);
        }
    }
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~ element processing functions ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

static void drawYLine(const std::shared_ptr<GRM::Element> &element, const std::shared_ptr<GRM::Context> &context)
{
  double window[4];
  double ymin;
  std::shared_ptr<GRM::Element> series = nullptr, line;
  std::string orientation = PLOT_DEFAULT_ORIENTATION;
  del_values del = del_values::update_without_default;

  auto subplot_element = getSubplotElement(element);
  window[0] = (double)subplot_element->getAttribute("window_xmin");
  window[1] = (double)subplot_element->getAttribute("window_xmax");
  window[2] = (double)subplot_element->getAttribute("window_ymin");
  window[3] = (double)subplot_element->getAttribute("window_ymax");

  for (const auto &child : subplot_element->children())
    {
      if (child->localName() != "series_barplot" && child->localName() != "series_stem") continue;
      series = child;
      break;
    }

  if (series != nullptr)
    {
      if (series->hasAttribute("orientation"))
        {
          orientation = static_cast<std::string>(series->getAttribute("orientation"));
        }
      else
        {
          series->setAttribute("orientation", orientation);
        }

      if (series->hasAttribute("yrange_min")) ymin = static_cast<double>(series->getAttribute("yrange_min"));
    }
  if (series != nullptr && series->localName() == "series_barplot" && ymin < 0) ymin = 0;

  int is_vertical = orientation == "vertical";

  del = del_values(static_cast<int>(element->getAttribute("_delete_children")));
  line = element->querySelectors("[name=\"yline\"]");

  double line_x1, line_x2, line_y1, line_y2;
  if (is_vertical)
    {
      line_x1 = ymin;
      line_x2 = ymin;
      line_y1 = window[2];
      line_y2 = window[3];
    }
  else
    {
      line_x1 = window[0];
      line_x2 = window[1];
      line_y1 = ymin;
      line_y2 = ymin;
    }

  if (del != del_values::update_without_default && del != del_values::update_with_default || line == nullptr)
    {
      line = global_render->createPolyline(line_x1, line_x2, line_y1, line_y2, 0, 0.0, 1);
      element->append(line);
    }
  else if (line != nullptr)
    {
      global_render->createPolyline(line_x1, line_x2, line_y1, line_y2, 0, 0.0, 1, line);
    }
  if (line != nullptr)
    {
      line->setAttribute("name", "yline");
      line->setAttribute("z_index", 4);
    }
}

static void axes(const std::shared_ptr<GRM::Element> &element, const std::shared_ptr<GRM::Context> &context)
{
  /*!
   * Processing function for axes
   *
   * \param[in] element The GRM::Element that contains the attributes and data keys
   * \param[in] context The GRM::Context that contains the actual data
   */
  double x_tick, x_org;
  double y_tick, y_org;
  int x_major, y_major;
  int tick_orientation = 1;
  double tick_size;
  std::string x_org_pos, y_org_pos;

  if (element->hasAttribute("x_org_pos"))
    {
      x_org_pos = static_cast<std::string>(element->getAttribute("x_org_pos"));
    }
  else
    {
      x_org_pos = "low";
    }
  if (element->hasAttribute("y_org_pos"))
    {
      y_org_pos = static_cast<std::string>(element->getAttribute("y_org_pos"));
    }
  else
    {
      y_org_pos = "low";
    }

  getAxesInformation(element, x_org_pos, y_org_pos, x_org, y_org, x_major, y_major, x_tick, y_tick);

  auto draw_axes_group = element->parentElement();
  auto subplot_element = getSubplotElement(element);

  if (element->hasAttribute("tick_orientation"))
    {
      tick_orientation = static_cast<int>(element->getAttribute("tick_orientation"));
    }

  getTickSize(element, tick_size);
  tick_size *= tick_orientation;

  if (redrawws) gr_axes(x_tick, y_tick, x_org, y_org, x_major, y_major, tick_size);
}

static void processAxes(const std::shared_ptr<GRM::Element> &element, const std::shared_ptr<GRM::Context> &context)
{
  auto subplot_element = getSubplotElement(element);

  if (element->hasAttribute("xlabel"))
    {
      processXlabel(element);
    }
  if (element->hasAttribute("ylabel"))
    {
      processYlabel(element);
    }

  auto pushAxesToZQueue = PushDrawableToZQueue(axes);
  pushAxesToZQueue(element, context);
}

static void axes3d(const std::shared_ptr<GRM::Element> &element, const std::shared_ptr<GRM::Context> &context)
{
  /*!
   * Processing function for axes3d
   *
   * \param[in] element The GRM::Element that contains the attributes and data keys
   * \param[in] context The GRM::Context that contains the actual data
   */
  double x_tick, x_org;
  double y_tick, y_org;
  double z_tick, z_org;
  int x_major, y_major, z_major;
  int tick_orientation = 1;
  double tick_size;
  std::string x_org_pos, y_org_pos, z_org_pos;

  if (element->hasAttribute("x_org_pos"))
    {
      x_org_pos = static_cast<std::string>(element->getAttribute("x_org_pos"));
    }
  else
    {
      x_org_pos = "low";
    }
  if (element->hasAttribute("y_org_pos"))
    {
      y_org_pos = static_cast<std::string>(element->getAttribute("y_org_pos"));
    }
  else
    {
      y_org_pos = "low";
    }
  if (element->hasAttribute("z_org_pos"))
    {
      z_org_pos = static_cast<std::string>(element->getAttribute("z_org_pos"));
    }
  else
    {
      z_org_pos = "low";
    }
  getAxes3dInformation(element, x_org_pos, y_org_pos, z_org_pos, x_org, y_org, z_org, x_major, y_major, z_major, x_tick,
                       y_tick, z_tick);

  auto subplot_element = getSubplotElement(element);

  if (element->hasAttribute("tick_orientation"))
    {
      tick_orientation = static_cast<int>(element->getAttribute("tick_orientation"));
    }

  getTickSize(element, tick_size);
  tick_size *= tick_orientation;

  if (redrawws) gr_axes3d(x_tick, y_tick, z_tick, x_org, y_org, z_org, x_major, y_major, z_major, tick_size);
}

static void processAxes3d(const std::shared_ptr<GRM::Element> &element, const std::shared_ptr<GRM::Context> &context)
{
  if (element->hasAttribute("xlabel"))
    {
      processXlabel(element);
    }
  if (element->hasAttribute("ylabel"))
    {
      processYlabel(element);
    }

  auto pushAxes3dToZqueue = PushDrawableToZQueue(axes3d);
  pushAxes3dToZqueue(element, context);
}

static void processCellArray(const std::shared_ptr<GRM::Element> &element, const std::shared_ptr<GRM::Context> &context)
{
  /*!
   * Processing function for cellArray
   *
   * \param[in] element The GRM::Element that contains the attributes and data keys
   * \param[in] context The GRM::Context that contains the actual data
   */
  double xmin = static_cast<double>(element->getAttribute("xmin"));
  double xmax = static_cast<double>(element->getAttribute("xmax"));
  double ymin = static_cast<double>(element->getAttribute("ymin"));
  double ymax = static_cast<double>(element->getAttribute("ymax"));
  int dimx = static_cast<int>(element->getAttribute("dimx"));
  int dimy = static_cast<int>(element->getAttribute("dimy"));
  int scol = static_cast<int>(element->getAttribute("scol"));
  int srow = static_cast<int>(element->getAttribute("srow"));
  int ncol = static_cast<int>(element->getAttribute("ncol"));
  int nrow = static_cast<int>(element->getAttribute("nrow"));
  auto color = static_cast<std::string>(element->getAttribute("color"));
  if (redrawws)
    gr_cellarray(xmin, xmax, ymin, ymax, dimx, dimy, scol, srow, ncol, nrow,
                 (int *)&(GRM::get<std::vector<int>>((*context)[color])[0]));
}

static void processColorbar(const std::shared_ptr<GRM::Element> &element, const std::shared_ptr<GRM::Context> &context)
{
  double c_min, c_max;
  int data, i;
  int options;
  unsigned int colors = static_cast<int>(element->getAttribute("colors"));
  del_values del = del_values::update_without_default;
  std::shared_ptr<GRM::Element> cellArray = nullptr, axes = nullptr;

  if (!getLimitsForColorbar(element, c_min, c_max))
    {
      auto subplot_element = getSubplotElement(element);
      if (!getLimitsForColorbar(subplot_element, c_min, c_max))
        {
          throw NotFoundError("Missing limits\n");
        }
    }

  gr_setwindow(0.0, 1.0, c_min, c_max);

  /* create cell array */
  std::vector<int> data_vec;
  for (i = 0; i < colors; ++i)
    {
      data = 1000 + (int)((255.0 * i) / (colors - 1) + 0.5);
      data_vec.push_back(data);
    }
  int id = (int)global_root->getAttribute("_id");
  std::string str = std::to_string(id);
  global_root->setAttribute("_id", id + 1);

  /* clear old child nodes */
  del = del_values(static_cast<int>(element->getAttribute("_delete_children")));
  clearOldChildren(&del, element);

  cellArray = element->querySelectors("cellarray");

  if (del != del_values::update_without_default && del != del_values::update_with_default && cellArray == nullptr)
    {
      cellArray =
          global_render->createCellArray(0, 1, c_max, c_min, 1, colors, 1, 1, 1, colors, "data" + str, data_vec);
      element->append(cellArray);
    }
  else if (cellArray != nullptr)
    {
      global_render->createCellArray(0, 1, c_max, c_min, 1, colors, 1, 1, 1, colors, "data" + str, data_vec, context,
                                     cellArray);
    }
  if (cellArray != nullptr) cellArray->setAttribute("name", "colorbar");

  /* create axes */
  gr_inqscale(&options);
  if (options & GR_OPTION_Z_LOG)
    {
      if ((del != del_values::update_without_default && del != del_values::update_with_default && axes == nullptr) ||
          !element->hasChildNodes())
        {
          axes = global_render->createAxes(0, 2, 1, c_min, 0, 1, 1);
          global_render->setLineColorInd(axes, 1);
          element->append(axes);
        }
      else if (axes != nullptr)
        {
          if (del == del_values::update_without_default)
            {
              axes->setAttribute("y_org", c_min);
            }
          else
            {
              global_render->createAxes(0, 2, 1, c_min, 0, 1, 1, axes);
            }
        }
      global_render->setScale(axes, GR_OPTION_Y_LOG);
    }
  else
    {
      double c_tick = auto_tick(c_min, c_max);
      if (del != del_values::update_without_default && del != del_values::update_with_default && axes == nullptr)
        {
          axes = global_render->createAxes(0, c_tick, 1, c_min, 0, 1, 1);
          global_render->setLineColorInd(axes, 1);
          element->append(axes);
        }
      else if (axes != nullptr)
        {
          if (del == del_values::update_without_default)
            {
              axes->setAttribute("y_tick", c_tick);
              axes->setAttribute("y_org", c_min);
            }
          else
            {
              global_render->createAxes(0, c_tick, 1, c_min, 0, 1, 1, axes);
            }
        }
    }
  if (del != del_values::update_without_default && axes != nullptr)
    {
      axes->setAttribute("tick_size", 0.005);
      axes->setAttribute("name", "colorbar");
    }
}

static void processBarplot(const std::shared_ptr<GRM::Element> &element, const std::shared_ptr<GRM::Context> &context)
{
  /*!
   * Processing function for barplot
   *
   * \param[in] element The GRM::Element that contains the attributes and data keys
   * \param[in] context The GRM::Context that contains the actual data
   */

  /* subplot level */
  int bar_color = 989, edge_color = 1;
  std::vector<double> bar_color_rgb = {-1, -1, -1};
  std::vector<double> edge_color_rgb = {-1, -1, -1};
  double bar_width = 0.8, edge_width = 1.0, bar_shift = 1;
  std::string style = "default", orientation = PLOT_DEFAULT_ORIENTATION;
  double wfac;
  int len_std_colors = 20;
  int std_colors[20] = {989, 982, 980, 981, 996, 983, 995, 988, 986, 990,
                        991, 984, 992, 993, 994, 987, 985, 997, 998, 999};
  int color_save_spot = PLOT_CUSTOM_COLOR_INDEX;
  unsigned int i;

  /* series level */
  unsigned int y_length;
  std::vector<int> c;
  unsigned int c_length;
  std::vector<double> c_rgb;
  unsigned int c_rgb_length;
  std::vector<std::string> ylabels;
  unsigned int ylabels_left = 0, ylabels_length = 0;
  int use_y_notations_from_inner_series = 1;
  /* Style Varianz */
  double pos_vertical_change = 0, neg_vertical_change = 0;
  double x1, x2, y1, y2;
  double x_min = 0, x_max, y_min = 0, y_max;
  int is_vertical;
  bool inner_series;
  bool inner_c = false;
  bool inner_c_rgb = false;
  del_values del = del_values::update_without_default;
  int child_id = 0;

  /* clear old bars */
  del = del_values(static_cast<int>(element->getAttribute("_delete_children")));
  clearOldChildren(&del, element);

  /* retrieve attributes from the subplot level */
  auto subplot = element->parentElement();
  int series_index = static_cast<int>(element->getAttribute("series_index"));
  int fixed_y_length = static_cast<int>(subplot->getAttribute("max_y_length"));

  if (element->hasAttribute("bar_color_rgb"))
    {
      auto bar_color_rgb_key = static_cast<std::string>(element->getAttribute("bar_color_rgb"));
      bar_color_rgb = GRM::get<std::vector<double>>((*context)[bar_color_rgb_key]);
    }
  if (element->hasAttribute("bar_color")) bar_color = static_cast<int>(element->getAttribute("bar_color"));
  if (element->hasAttribute("bar_width")) bar_width = static_cast<double>(element->getAttribute("bar_width"));
  if (element->hasAttribute("style"))
    {
      style = static_cast<std::string>(element->getAttribute("style"));
    }
  else
    {
      element->setAttribute("style", style);
    }
  if (element->hasAttribute("orientation"))
    {
      orientation = static_cast<std::string>(element->getAttribute("orientation"));
    }
  else
    {
      element->setAttribute("orientation", orientation);
    }

  is_vertical = orientation == "vertical";

  if (bar_color_rgb[0] != -1)
    {
      for (i = 0; i < 3; i++)
        {
          if (bar_color_rgb[i] > 1 || bar_color_rgb[i] < 0)
            throw std::out_of_range("For barplot series bar_color_rgb must be inside [0, 1].\n");
        }
    }

  /* retrieve attributes form the series level */
  if (!element->hasAttribute("y")) throw NotFoundError("Barplot series is missing y.\n");

  auto y_key = static_cast<std::string>(element->getAttribute("y"));
  std::vector<double> y_vec = GRM::get<std::vector<double>>((*context)[y_key]);
  y_length = size(y_vec);

  if (!element->hasAttribute("indices")) throw NotFoundError("Barplot series is missing indices\n");
  auto indices = static_cast<std::string>(element->getAttribute("indices"));
  std::vector<int> indices_vec = GRM::get<std::vector<int>>((*context)[indices]);

  inner_series = size(indices_vec) != y_length;

  wfac = 0.9 * bar_width;

  if (element->hasAttribute("edge_color_rgb"))
    {
      auto edge_color_rgb_key = static_cast<std::string>(element->getAttribute("edge_color_rgb"));
      edge_color_rgb = GRM::get<std::vector<double>>((*context)[edge_color_rgb_key]);
    }
  if (element->hasAttribute("edge_color")) edge_color = static_cast<int>(element->getAttribute("edge_color"));
  if (element->hasAttribute("edge_width")) edge_width = static_cast<double>(element->getAttribute("edge_width"));
  global_render->setTextAlign(element, 2, 3);
  global_render->selectClipXForm(element, 1);

  if (edge_color_rgb[0] != -1)
    {
      for (i = 0; i < 3; i++)
        {
          if (edge_color_rgb[i] > 1 || edge_color_rgb[i] < 0)
            throw std::out_of_range("For barplot series edge_color_rgb must be inside [0, 1].\n");
        }
    }

  if (element->hasAttribute("c"))
    {
      auto c_key = static_cast<std::string>(element->getAttribute("c"));
      c = GRM::get<std::vector<int>>((*context)[c_key]);
      c_length = size(c);
    }
  if (element->hasAttribute("c_rgb"))
    {
      auto c_rgb_key = static_cast<std::string>(element->getAttribute("c_rgb"));
      c_rgb = GRM::get<std::vector<double>>((*context)[c_rgb_key]);
      c_rgb_length = size(c_rgb);
    }
  if (element->hasAttribute("ylabels"))
    {
      auto ylabels_key = static_cast<std::string>(element->getAttribute("ylabels"));
      ylabels = GRM::get<std::vector<std::string>>((*context)[ylabels_key]);
      ylabels_length = size(ylabels);

      ylabels_left = ylabels_length;
    }

  if (element->hasAttribute("xrange_min") && element->hasAttribute("xrange_max") &&
      element->hasAttribute("yrange_min") && element->hasAttribute("yrange_max"))
    {
      x_min = static_cast<double>(element->getAttribute("xrange_min"));
      x_max = static_cast<double>(element->getAttribute("xrange_max"));
      y_min = static_cast<double>(element->getAttribute("yrange_min"));
      y_max = static_cast<double>(element->getAttribute("yrange_max"));
      if (!element->hasAttribute("bar_width"))
        {
          bar_width = (x_max - x_min) / (y_length - 1.0);
          bar_shift = (x_max - x_min) / (y_length - 1.0);
          x_min -= 1; // in the later calculation there is allways a +1 in combination with x
          wfac = 0.9 * bar_width;
        }
    }

  if (style != "lined" && inner_series) throw TypeError("Unsupported operation for barplot series.\n");
  if (!c.empty())
    {
      if (!inner_series && (c_length < y_length))
        throw std::length_error("For a barplot series c_length must be >= y_length.\n");
      if (inner_series)
        {
          if (c_length == y_length)
            {
              inner_c = true;
            }
          else if (c_length != size(indices_vec))
            {
              throw std::length_error("For a barplot series c_length must be >= y_length.\n");
            }
        }
    }
  if (!c_rgb.empty())
    {
      if (!inner_series && (c_rgb_length < y_length * 3))
        throw std::length_error("For a barplot series c_rgb_length must be >= y_length * 3.\n");
      if (inner_series)
        {
          if (c_rgb_length == y_length * 3)
            {
              inner_c_rgb = true;
            }
          else if (c_rgb_length != size(indices_vec) * 3)
            {
              throw std::length_error("For a barplot series c_rgb_length must be >= y_length * 3\n");
            }
        }
      for (i = 0; i < y_length * 3; i++)
        {
          if ((c_rgb[i] > 1 || c_rgb[i] < 0) && c_rgb[i] != -1)
            throw std::out_of_range("For barplot series c_rgb must be inside [0, 1] or -1.\n");
        }
    }
  if (!ylabels.empty())
    {
      use_y_notations_from_inner_series = 0;
    }

  global_render->setFillIntStyle(element, 1);
  processFillIntStyle(element);
  global_render->setFillColorInd(element, bar_color);
  processFillColorInd(element);

  /* overrides bar_color */
  if (bar_color_rgb[0] != -1)
    {
      global_render->setColorRep(element, color_save_spot, bar_color_rgb[0], bar_color_rgb[1], bar_color_rgb[2]);
      processColorReps(element);
      bar_color = color_save_spot;
      global_render->setFillColorInd(element, bar_color);
      processFillColorInd(element);
    }
  if (!inner_series)
    {
      /* Draw Bar */
      for (i = 0; i < y_length; i++)
        {
          y1 = y_min;
          y2 = y_vec[i];

          if (style == "default")
            {
              x1 = (i * bar_shift) + 1 - 0.5 * bar_width;
              x2 = (i * bar_shift) + 1 + 0.5 * bar_width;
            }
          else if (style == "stacked")
            {
              x1 = series_index + 1 - 0.5 * bar_width;
              x2 = series_index + 1 + 0.5 * bar_width;
              if (y_vec[i] > 0)
                {
                  y1 = ((i == 0) ? y_min : 0) + pos_vertical_change;
                  pos_vertical_change += y_vec[i] - ((i > 0) ? y_min : 0);
                  y2 = pos_vertical_change;
                }
              else
                {
                  y1 = ((i == 0) ? y_min : 0) + neg_vertical_change;
                  neg_vertical_change += y_vec[i] - ((i > 0) ? y_min : 0);
                  y2 = neg_vertical_change;
                }
            }
          else if (style == "lined")
            {
              bar_width = wfac / y_length;
              x1 = series_index + 1 - 0.5 * wfac + bar_width * i;
              x2 = series_index + 1 - 0.5 * wfac + bar_width + bar_width * i;
            }
          x1 += x_min;
          x2 += x_min;

          if (is_vertical)
            {
              double tmp1 = x1, tmp2 = x2;
              x1 = y1, x2 = y2;
              y1 = tmp1, y2 = tmp2;
            }

          int fillcolorind = -1;
          std::vector<double> bar_fillcolor_rgb, edge_fillcolor_rgb;
          std::string bar_color_rgb_key, edge_color_rgb_key;
          std::shared_ptr<GRM::Element> bar;
          int id = static_cast<int>(global_root->getAttribute("_id"));
          std::string str = std::to_string(id);

          /* attributes for fillrect */
          if (style != "default")
            {
              fillcolorind = std_colors[i % len_std_colors];
            }
          if (!c.empty() && c[i] != -1)
            {
              fillcolorind = c[i];
            }
          else if (!c_rgb.empty() && c_rgb[i * 3] != -1)
            {
              bar_fillcolor_rgb = std::vector<double>{c_rgb[i * 3], c_rgb[i * 3 + 1], c_rgb[i * 3 + 2]};
              bar_color_rgb_key = "bar_color_rgb" + str;
              (*context)[bar_color_rgb_key] = bar_fillcolor_rgb;
            }

          if (fillcolorind == -1)
            fillcolorind =
                element->hasAttribute("fillcolorind") ? static_cast<int>(element->getAttribute("fillcolorind")) : 989;

          /* Colorrep for drawrect */
          if (edge_color_rgb[0] != -1)
            {
              edge_fillcolor_rgb = std::vector<double>{edge_color_rgb[0], edge_color_rgb[1], edge_color_rgb[2]};
              edge_color_rgb_key = "edge_color_rgb" + str;
              (*context)[edge_color_rgb_key] = edge_fillcolor_rgb;
            }

          /* Create bars */
          if (del != del_values::update_without_default && del != del_values::update_with_default)
            {
              bar = global_render->createBar(x1, x2, y1, y2, fillcolorind, edge_color, bar_color_rgb_key,
                                             edge_color_rgb_key, edge_width, "");
              bar->setAttribute("_child_id", child_id++);
              element->append(bar);
            }
          else
            {
              bar = element->querySelectors("[_child_id=" + std::to_string(child_id++) + "]");
              if (bar != nullptr)
                global_render->createBar(x1, x2, y1, y2, fillcolorind, edge_color, bar_color_rgb_key,
                                         edge_color_rgb_key, edge_width, "", bar);
            }

          /* Draw ynotations */
          if (!ylabels.empty() && ylabels_left > 0)
            {
              if (bar != nullptr) bar->setAttribute("text", ylabels[i]);
              --ylabels_left;
            }
          global_root->setAttribute("_id", ++id);
        }
    }
  else
    {
      /* edge has the same with and color for every inner series */
      global_render->setLineWidth(element, edge_width);
      if (edge_color_rgb[0] != -1)
        {
          global_render->setColorRep(element, color_save_spot, edge_color_rgb[0], edge_color_rgb[1], edge_color_rgb[2]);
          processFillColorInd(element);
          edge_color = color_save_spot;
        }
      global_render->setLineColorInd(element, edge_color);
      element->setAttribute("edge_color", edge_color);
      processLineWidth(element);
      processLineColorInd(element);

      int inner_y_start_index = 0;
      /* Draw inner_series */
      for (int inner_series_index = 0; inner_series_index < size(indices_vec); inner_series_index++)
        {
          /* Draw bars from inner_series */
          int inner_y_length = indices_vec[inner_series_index];
          std::vector<double> inner_y_vec(y_vec.begin() + inner_y_start_index,
                                          y_vec.begin() + inner_y_start_index + inner_y_length);
          bar_width = wfac / fixed_y_length;

          for (i = 0; i < inner_y_length; i++)
            {
              x1 = series_index + 1 - 0.5 * wfac + bar_width * inner_series_index;
              x2 = series_index + 1 - 0.5 * wfac + bar_width + bar_width * inner_series_index;
              if (inner_y_vec[i] > 0)
                {
                  y1 = ((i == 0) ? y_min : 0) + pos_vertical_change;
                  pos_vertical_change += inner_y_vec[i] - ((i > 0) ? y_min : 0);
                  y2 = pos_vertical_change;
                }
              else
                {
                  y1 = ((i == 0) ? y_min : 0) + neg_vertical_change;
                  neg_vertical_change += inner_y_vec[i] - ((i > 0) ? y_min : 0);
                  y2 = neg_vertical_change;
                }
              x1 += x_min;
              x2 += x_min;

              if (is_vertical)
                {
                  double tmp1 = x1, tmp2 = x2;
                  x1 = y1, x2 = y2;
                  y1 = tmp1, y2 = tmp2;
                }

              int fillcolorind = -1;
              std::vector<double> bar_fillcolor_rgb, edge_fillcolor_rgb;
              std::string bar_color_rgb_key, edge_color_rgb_key;
              std::shared_ptr<GRM::Element> bar;
              int id = static_cast<int>(global_root->getAttribute("_id"));
              std::string str = std::to_string(id);

              /* attributes for fillrect */
              if (!c.empty() && !inner_c && c[inner_series_index] != -1)
                {
                  fillcolorind = c[inner_series_index];
                }
              if (!c_rgb.empty() && !inner_c_rgb && c_rgb[inner_series_index * 3] != -1)
                {
                  bar_fillcolor_rgb =
                      std::vector<double>{c_rgb[inner_series_index * 3], c_rgb[inner_series_index * 3 + 1],
                                          c_rgb[inner_series_index * 3 + 2]};
                  bar_color_rgb_key = "bar_color_rgb" + str;
                  (*context)[bar_color_rgb_key] = bar_fillcolor_rgb;
                }
              if (inner_c && c[inner_y_start_index + i] != -1)
                {
                  fillcolorind = c[inner_y_start_index + i];
                }
              if (inner_c_rgb && c_rgb[(inner_y_start_index + i) * 3] != -1)
                {
                  bar_fillcolor_rgb = std::vector<double>{c_rgb[(inner_y_start_index + i) * 3],
                                                          c_rgb[(inner_y_start_index + i) * 3 + 1],
                                                          c_rgb[(inner_y_start_index + i) * 3 + 2]};
                  bar_color_rgb_key = "bar_color_rgb" + str;
                  (*context)[bar_color_rgb_key] = bar_fillcolor_rgb;
                }
              if (fillcolorind == -1) fillcolorind = std_colors[inner_series_index % len_std_colors];

              /* Create bars */
              if (del != del_values::update_without_default && del != del_values::update_with_default)
                {
                  bar = global_render->createBar(x1, x2, y1, y2, fillcolorind, edge_color, bar_color_rgb_key,
                                                 edge_color_rgb_key, edge_width, "");
                  bar->setAttribute("_child_id", child_id++);
                  element->append(bar);
                }
              else
                {
                  bar = element->querySelectors("[_child_id=" + std::to_string(child_id++) + "]");
                  if (bar != nullptr)
                    global_render->createBar(x1, x2, y1, y2, fillcolorind, edge_color, bar_color_rgb_key,
                                             edge_color_rgb_key, edge_width, "", bar);
                }

              /* Draw ynotations from inner_series */
              if (!ylabels.empty() && ylabels_left > 0)
                {
                  if (bar != nullptr) bar->setAttribute("text", ylabels[ylabels_length - ylabels_left]);
                  --ylabels_left;
                }
              global_root->setAttribute("_id", ++id);
            }
          pos_vertical_change = 0;
          neg_vertical_change = 0;
          y_length = 0;
          inner_y_start_index += inner_y_length;
        }
    }
  element->setAttribute("linecolorind", edge_color);
  processLineColorInd(element);

  // errorbar handling
  for (const auto &child : element->children())
    {
      if (child->localName() == "errorbars")
        {
          std::vector<double> bar_centers;
          bar_width = wfac / y_length;
          for (i = 0; i < y_length; i++)
            {
              if (style == "default")
                {
                  x1 = x_min + (i * bar_shift) + 1 - 0.5 * bar_width;
                  x2 = x_min + (i * bar_shift) + 1 + 0.5 * bar_width;
                }
              else if (style == "lined")
                {
                  x1 = x_min + series_index + 1 - 0.5 * wfac + bar_width * i;
                  x2 = x_min + series_index + 1 - 0.5 * wfac + bar_width + bar_width * i;
                }
              else
                {
                  x1 = x_min + series_index + 1 - 0.5 * bar_width;
                  x2 = x_min + series_index + 1 + 0.5 * bar_width;
                }
              bar_centers.push_back((x1 + x2) / 2.0);
            }
          extendErrorbars(child, context, bar_centers, y_vec);
        }
    }
}

static void processContour(const std::shared_ptr<GRM::Element> &element, const std::shared_ptr<GRM::Context> &context)
{
  /*!
   * Processing function for contour
   *
   * \param[in] element The GRM::Element that contains the attributes and data keys
   * \param[in] context The GRM::Context that contains the actual data
   */
  double z_min, z_max;
  int num_levels = PLOT_DEFAULT_CONTOUR_LEVELS;
  int i;
  unsigned int x_length, y_length, z_length;
  std::vector<double> x_vec, y_vec, z_vec;
  std::vector<double> px_vec, py_vec, pz_vec;
  int major_h = 1000;
  auto plot_parent = element->parentElement();

  getPlotParent(plot_parent);
  z_min = static_cast<double>(plot_parent->getAttribute("_zlim_min"));
  z_max = static_cast<double>(plot_parent->getAttribute("_zlim_max"));
  if (element->hasAttribute("levels"))
    {
      num_levels = static_cast<int>(element->getAttribute("levels"));
    }
  else
    {
      element->setAttribute("levels", num_levels);
    }

  gr_setprojectiontype(0);
  gr_setspace(z_min, z_max, 0, 90);

  std::vector<double> h(num_levels);

  if (!element->hasAttribute("px") || !element->hasAttribute("py") || !element->hasAttribute("pz"))
    {
      if (!element->hasAttribute("x")) throw NotFoundError("Contour series is missing required attribute x-data.\n");
      auto x = static_cast<std::string>(element->getAttribute("x"));
      if (!element->hasAttribute("y")) throw NotFoundError("Contour series is missing required attribute y-data.\n");
      auto y = static_cast<std::string>(element->getAttribute("y"));
      if (!element->hasAttribute("z")) throw NotFoundError("Contour series is missing required attribute z-data.\n");
      auto z = static_cast<std::string>(element->getAttribute("z"));

      x_vec = GRM::get<std::vector<double>>((*context)[x]);
      y_vec = GRM::get<std::vector<double>>((*context)[y]);
      z_vec = GRM::get<std::vector<double>>((*context)[z]);
      x_length = x_vec.size();
      y_length = y_vec.size();
      z_length = z_vec.size();

      int id = (int)global_root->getAttribute("_id");
      global_root->setAttribute("_id", id + 1);
      std::string str = std::to_string(id);

      if (x_length == y_length && x_length == z_length)
        {
          std::vector<double> gridit_x_vec(PLOT_CONTOUR_GRIDIT_N);
          std::vector<double> gridit_y_vec(PLOT_CONTOUR_GRIDIT_N);
          std::vector<double> gridit_z_vec(PLOT_CONTOUR_GRIDIT_N * PLOT_CONTOUR_GRIDIT_N);

          double *gridit_x = &(gridit_x_vec[0]);
          double *gridit_y = &(gridit_y_vec[0]);
          double *gridit_z = &(gridit_z_vec[0]);
          double *x_p = &(x_vec[0]);
          double *y_p = &(y_vec[0]);
          double *z_p = &(z_vec[0]);

          gr_gridit(x_length, x_p, y_p, z_p, PLOT_CONTOUR_GRIDIT_N, PLOT_CONTOUR_GRIDIT_N, gridit_x, gridit_y,
                    gridit_z);
          for (i = 0; i < PLOT_CONTOUR_GRIDIT_N * PLOT_CONTOUR_GRIDIT_N; i++)
            {
              z_min = grm_min(gridit_z[i], z_min);
              z_max = grm_max(gridit_z[i], z_max);
            }

          global_render->setSpace(element->parentElement(), z_min, z_max, 0, 90);
          processSpace(element->parentElement());

          px_vec = std::vector<double>(gridit_x, gridit_x + PLOT_CONTOUR_GRIDIT_N);
          py_vec = std::vector<double>(gridit_y, gridit_y + PLOT_CONTOUR_GRIDIT_N);
          pz_vec = std::vector<double>(gridit_z, gridit_z + PLOT_CONTOUR_GRIDIT_N * PLOT_CONTOUR_GRIDIT_N);
        }
      else
        {
          if (x_length * y_length != z_length)
            throw std::length_error("For contour series x_length * y_length must be z_length.\n");

          px_vec = x_vec;
          py_vec = y_vec;
          pz_vec = z_vec;
        }

      (*context)["px" + str] = px_vec;
      element->setAttribute("px", "px" + str);
      (*context)["py" + str] = py_vec;
      element->setAttribute("py", "py" + str);
      (*context)["pz" + str] = pz_vec;
      element->setAttribute("pz", "pz" + str);
    }
  else
    {
      auto px = static_cast<std::string>(element->getAttribute("px"));
      auto py = static_cast<std::string>(element->getAttribute("py"));
      auto pz = static_cast<std::string>(element->getAttribute("pz"));

      px_vec = GRM::get<std::vector<double>>((*context)[px]);
      py_vec = GRM::get<std::vector<double>>((*context)[py]);
      pz_vec = GRM::get<std::vector<double>>((*context)[pz]);
    }

  for (i = 0; i < num_levels; ++i)
    {
      h[i] = z_min + (1.0 * i) / num_levels * (z_max - z_min);
    }

  int nx = px_vec.size();
  int ny = py_vec.size();

  double *px_p = &(px_vec[0]);
  double *py_p = &(py_vec[0]);
  double *h_p = &(h[0]);
  double *pz_p = &(pz_vec[0]);

  if (redrawws) gr_contour(nx, ny, num_levels, px_p, py_p, h_p, pz_p, major_h);
}

static void processContourf(const std::shared_ptr<GRM::Element> &element, const std::shared_ptr<GRM::Context> &context)
{
  /*!
   * Processing function for contourf
   *
   * \param[in] element The GRM::Element that contains the attributes and data keys
   * \param[in] context The GRM::Context that contains the actual data
   */
  double z_min, z_max;
  int num_levels = PLOT_DEFAULT_CONTOUR_LEVELS;
  int i;
  unsigned int x_length, y_length, z_length;
  std::vector<double> x_vec, y_vec, z_vec;
  std::vector<double> px_vec, py_vec, pz_vec;
  int major_h = 0;
  auto plot_parent = element->parentElement();

  getPlotParent(plot_parent);
  z_min = static_cast<double>(plot_parent->getAttribute("_zlim_min"));
  z_max = static_cast<double>(plot_parent->getAttribute("_zlim_max"));
  if (element->hasAttribute("levels"))
    {
      num_levels = static_cast<int>(element->getAttribute("levels"));
    }
  else
    {
      element->setAttribute("levels", num_levels);
    }

  global_render->setProjectionType(element->parentElement(), 0);
  global_render->setSpace(element->parentElement(), z_min, z_max, 0, 90);

  std::vector<double> h(num_levels);

  if (!element->hasAttribute("px") || !element->hasAttribute("py") || !element->hasAttribute("pz"))
    {
      if (!element->hasAttribute("x")) throw NotFoundError("Contourf series is missing required attribute x-data.\n");
      auto x = static_cast<std::string>(element->getAttribute("x"));
      if (!element->hasAttribute("y")) throw NotFoundError("Contourf series is missing required attribute y-data.\n");
      auto y = static_cast<std::string>(element->getAttribute("y"));
      if (!element->hasAttribute("z")) throw NotFoundError("Contourf series is missing required attribute z-data.\n");
      auto z = static_cast<std::string>(element->getAttribute("z"));

      x_vec = GRM::get<std::vector<double>>((*context)[x]);
      y_vec = GRM::get<std::vector<double>>((*context)[y]);
      z_vec = GRM::get<std::vector<double>>((*context)[z]);
      x_length = x_vec.size();
      y_length = y_vec.size();
      z_length = z_vec.size();

      int id = (int)global_root->getAttribute("_id");
      global_root->setAttribute("_id", id + 1);
      std::string str = std::to_string(id);

      gr_setlinecolorind(1);
      global_render->setLineColorInd(element, 1);

      if (x_length == y_length && x_length == z_length)
        {
          std::vector<double> gridit_x_vec(PLOT_CONTOUR_GRIDIT_N);
          std::vector<double> gridit_y_vec(PLOT_CONTOUR_GRIDIT_N);
          std::vector<double> gridit_z_vec(PLOT_CONTOUR_GRIDIT_N * PLOT_CONTOUR_GRIDIT_N);

          double *gridit_x = &(gridit_x_vec[0]);
          double *gridit_y = &(gridit_y_vec[0]);
          double *gridit_z = &(gridit_z_vec[0]);
          double *x_p = &(x_vec[0]);
          double *y_p = &(y_vec[0]);
          double *z_p = &(z_vec[0]);

          gr_gridit(x_length, x_p, y_p, z_p, PLOT_CONTOUR_GRIDIT_N, PLOT_CONTOUR_GRIDIT_N, gridit_x, gridit_y,
                    gridit_z);
          for (i = 0; i < PLOT_CONTOUR_GRIDIT_N * PLOT_CONTOUR_GRIDIT_N; i++)
            {
              z_min = grm_min(gridit_z[i], z_min);
              z_max = grm_max(gridit_z[i], z_max);
            }

          global_render->setLineColorInd(element, 989);

          px_vec = std::vector<double>(gridit_x, gridit_x + PLOT_CONTOUR_GRIDIT_N);
          py_vec = std::vector<double>(gridit_y, gridit_y + PLOT_CONTOUR_GRIDIT_N);
          pz_vec = std::vector<double>(gridit_z, gridit_z + PLOT_CONTOUR_GRIDIT_N * PLOT_CONTOUR_GRIDIT_N);
        }
      else
        {
          if (x_length * y_length != z_length)
            throw std::length_error("For contourf series x_length * y_length must be z_length.\n");

          global_render->setLineColorInd(element, 989);

          px_vec = x_vec;
          py_vec = y_vec;
          pz_vec = z_vec;
        }

      (*context)["px" + str] = px_vec;
      element->setAttribute("px", "px" + str);
      (*context)["py" + str] = py_vec;
      element->setAttribute("py", "py" + str);
      (*context)["pz" + str] = pz_vec;
      element->setAttribute("pz", "pz" + str);
      processLineColorInd(element);
    }
  else
    {
      auto px = static_cast<std::string>(element->getAttribute("px"));
      auto py = static_cast<std::string>(element->getAttribute("py"));
      auto pz = static_cast<std::string>(element->getAttribute("pz"));

      px_vec = GRM::get<std::vector<double>>((*context)[px]);
      py_vec = GRM::get<std::vector<double>>((*context)[py]);
      pz_vec = GRM::get<std::vector<double>>((*context)[pz]);
    }

  for (i = 0; i < num_levels; ++i)
    {
      h[i] = z_min + (1.0 * i) / num_levels * (z_max - z_min);
    }

  int nx = px_vec.size();
  int ny = py_vec.size();

  double *px_p = &(px_vec[0]);
  double *py_p = &(py_vec[0]);
  double *h_p = &(h[0]);
  double *pz_p = &(pz_vec[0]);

  if (redrawws) gr_contourf(nx, ny, num_levels, px_p, py_p, h_p, pz_p, major_h);
}

static void processDrawArc(const std::shared_ptr<GRM::Element> &element, const std::shared_ptr<GRM::Context> &context)
{
  /*!
   * Processing function for drawArc
   *
   * \param[in] element The GRM::Element that contains the attributes and data keys
   * \param[in] context The GRM::Context that contains the actual data
   */
  double xmin = static_cast<double>(element->getAttribute("xmin"));
  double xmax = static_cast<double>(element->getAttribute("xmax"));
  double ymin = static_cast<double>(element->getAttribute("ymin"));
  double ymax = static_cast<double>(element->getAttribute("ymax"));
  double a1 = static_cast<double>(element->getAttribute("a1"));
  double a2 = static_cast<double>(element->getAttribute("a2"));
  if (redrawws) gr_drawarc(xmin, xmax, ymin, ymax, a1, a2);
}

static void processDrawGraphics(const std::shared_ptr<GRM::Element> &element,
                                const std::shared_ptr<GRM::Context> &context)
{
  auto key = (std::string)element->getAttribute("data");
  auto data_vec = GRM::get<std::vector<int>>((*context)[key]);

  std::vector<char> char_vec;
  char_vec.reserve(data_vec.size());
  for (int i : data_vec)
    {
      char_vec.push_back((char)i);
    }
  char *data_p = &(char_vec[0]);

  if (redrawws) gr_drawgraphics(data_p);
}

static void processDrawImage(const std::shared_ptr<GRM::Element> &element, const std::shared_ptr<GRM::Context> &context)
{
  /*!
   * Processing function for drawImage
   *
   * \param[in] element The GRM::Element that contains the attributes and data keys
   * \param[in] context The GRM::Context that contains the actual data
   */
  double xmin = static_cast<double>(element->getAttribute("xmin"));
  double xmax = static_cast<double>(element->getAttribute("xmax"));
  double ymin = static_cast<double>(element->getAttribute("ymin"));
  double ymax = static_cast<double>(element->getAttribute("ymax"));
  int width = static_cast<int>(element->getAttribute("width"));
  int height = static_cast<int>(element->getAttribute("height"));
  int model = static_cast<int>(element->getAttribute("model"));
  auto data = static_cast<std::string>(element->getAttribute("data"));
  if (redrawws)
    gr_drawimage(xmin, xmax, ymax, ymin, width, height, (int *)&(GRM::get<std::vector<int>>((*context)[data])[0]),
                 model);
}

static void processErrorbars(const std::shared_ptr<GRM::Element> &element, const std::shared_ptr<GRM::Context> &context)
{
  std::string orientation = PLOT_DEFAULT_ORIENTATION, kind;
  int is_horizontal;
  std::vector<double> absolute_upwards_vec, absolute_downwards_vec, relative_upwards_vec, relative_downwards_vec;
  std::string absolute_upwards, absolute_downwards, relative_upwards, relative_downwards;
  double absolute_upwards_flt, relative_upwards_flt, absolute_downwards_flt, relative_downwards_flt;
  unsigned int upwards_length, downwards_length, i;
  int scale_options, color_upwardscap, color_downwardscap, color_errorbar;
  double marker_size, xmin, xmax, ymin, ymax, tick, a, b, e_upwards, e_downwards, x_value;
  double line_x[2], line_y[2];
  std::vector<double> x_vec, y_vec;
  unsigned int x_length;
  std::string x, y;
  std::shared_ptr<GRM::Element> series;
  del_values del = del_values::update_without_default;
  int child_id = 0;

  absolute_upwards_flt = absolute_downwards_flt = relative_upwards_flt = relative_downwards_flt = FLT_MAX;
  if (static_cast<std::string>(element->parentElement()->parentElement()->localName()) == "plot")
    {
      series = element->parentElement();
    }
  else
    {
      series = element->parentElement()->parentElement(); // marginalheatmap
    }

  if (!element->hasAttribute("x")) throw NotFoundError("Errorbars are missing required attribute x-data.\n");
  x = static_cast<std::string>(element->getAttribute("x"));
  if (!element->hasAttribute("y")) throw NotFoundError("Errorbars are missing required attribute y-data.\n");
  y = static_cast<std::string>(element->getAttribute("y"));

  x_vec = GRM::get<std::vector<double>>((*context)[x]);
  y_vec = GRM::get<std::vector<double>>((*context)[y]);
  x_length = x_vec.size();
  kind = static_cast<std::string>(series->parentElement()->getAttribute("kind"));
  if (element->parentElement()->hasAttribute("orientation"))
    {
      orientation = static_cast<std::string>(element->parentElement()->getAttribute("orientation"));
    }
  else
    {
      element->parentElement()->setAttribute("orientation", orientation);
    }

  if (!element->hasAttribute("absolute_downwards") && !element->hasAttribute("relative_downwards"))
    throw NotFoundError("Errorbars are missing required attribute downwards.\n");
  if (!element->hasAttribute("absolute_upwards") && !element->hasAttribute("relative_upwards"))
    throw NotFoundError("Errorbars are missing required attribute upwards.\n");
  if (element->hasAttribute("absolute_downwards"))
    {
      absolute_downwards = static_cast<std::string>(element->getAttribute("absolute_downwards"));
      absolute_downwards_vec = GRM::get<std::vector<double>>((*context)[absolute_downwards]);
      downwards_length = absolute_downwards_vec.size();
    }
  if (element->hasAttribute("relative_downwards"))
    {
      relative_downwards = static_cast<std::string>(element->getAttribute("relative_downwards"));
      relative_downwards_vec = GRM::get<std::vector<double>>((*context)[relative_downwards]);
      downwards_length = absolute_downwards_vec.size();
    }
  if (element->hasAttribute("absolute_upwards"))
    {
      absolute_upwards = static_cast<std::string>(element->getAttribute("absolute_upwards"));
      absolute_upwards_vec = GRM::get<std::vector<double>>((*context)[absolute_upwards]);
      upwards_length = absolute_upwards_vec.size();
    }
  if (element->hasAttribute("relative_upwards"))
    {
      relative_upwards = static_cast<std::string>(element->getAttribute("relative_upwards"));
      relative_upwards_vec = GRM::get<std::vector<double>>((*context)[relative_upwards]);
      upwards_length = absolute_upwards_vec.size();
    }
  if (element->hasAttribute("absolute_downwards_flt"))
    absolute_downwards_flt = static_cast<double>(element->getAttribute("absolute_downwards_flt"));
  if (element->hasAttribute("absolute_upwards_flt"))
    absolute_upwards_flt = static_cast<double>(element->getAttribute("absolute_upwards_flt"));
  if (element->hasAttribute("relative_downwards_flt"))
    relative_downwards_flt = static_cast<double>(element->getAttribute("relative_downwards_flt"));
  if (element->hasAttribute("relative_upwards_flt"))
    relative_upwards_flt = static_cast<double>(element->getAttribute("relative_upwards_flt"));

  if (absolute_upwards_vec.empty() && relative_upwards_vec.empty() && absolute_upwards_flt == FLT_MAX &&
      relative_upwards_flt == FLT_MAX && absolute_downwards_vec.empty() && relative_downwards_vec.empty() &&
      absolute_downwards_flt == FLT_MAX && relative_downwards_flt == FLT_MAX)
    {
      throw NotFoundError("Errorbar is missing required error-data.");
    }

  is_horizontal = orientation == "horizontal";

  /* Getting GRM options and sizes. See gr_verrorbars. */
  gr_savestate();
  gr_inqmarkersize(&marker_size);
  gr_inqwindow(&xmin, &xmax, &ymin, &ymax);
  gr_inqscale(&scale_options);
  tick = marker_size * 0.0075 * (xmax - xmin);
  a = (xmax - xmin) / log10(xmax / xmin);
  b = xmin - a * log10(xmin);

  gr_inqlinecolorind(&color_errorbar);
  // special case for barplot
  if (kind == "barplot") color_errorbar = static_cast<int>(element->parentElement()->getAttribute("edge_color"));
  color_upwardscap = color_downwardscap = color_errorbar;
  if (element->hasAttribute("upwardscap_color"))
    color_upwardscap = static_cast<int>(element->getAttribute("upwardscap_color"));
  if (element->hasAttribute("downwardscap_color"))
    color_downwardscap = static_cast<int>(element->getAttribute("downwardscap_color"));
  if (element->hasAttribute("errorbar_color"))
    color_errorbar = static_cast<int>(element->getAttribute("errorbar_color"));

  /* clear old lines */
  del = del_values(static_cast<int>(element->getAttribute("_delete_children")));
  clearOldChildren(&del, element);

  /* Actual drawing of bars */
  e_upwards = e_downwards = FLT_MAX;
  for (i = 0; i < x_length; i++)
    {
      std::shared_ptr<GRM::Element> errorbar;
      if (!absolute_upwards.empty() || !relative_upwards.empty() || absolute_upwards_flt != FLT_MAX ||
          relative_upwards_flt != FLT_MAX)
        {
          e_upwards = y_vec[i] * (1. + (!relative_upwards.empty()
                                            ? relative_upwards_vec[i]
                                            : (relative_upwards_flt != FLT_MAX ? relative_upwards_flt : 0))) +
                      (!absolute_upwards.empty() ? absolute_upwards_vec[i]
                                                 : (absolute_upwards_flt != FLT_MAX ? absolute_upwards_flt : 0.));
        }

      if (!absolute_downwards.empty() || !relative_downwards.empty() || absolute_downwards_flt != FLT_MAX ||
          relative_downwards_flt != FLT_MAX)
        {
          e_downwards =
              y_vec[i] * (1. - (!relative_downwards.empty()
                                    ? relative_downwards_vec[i]
                                    : (relative_downwards_flt != FLT_MAX ? relative_downwards_flt : 0))) -
              (!absolute_downwards.empty() ? absolute_downwards_vec[i]
                                           : (absolute_downwards_flt != FLT_MAX ? absolute_downwards_flt : 0.));
        }

      /* See gr_verrorbars for reference */
      x_value = x_vec[i];
      line_x[0] = X_LOG(X_LIN(x_value - tick, scale_options, xmin, xmax, a, b), scale_options, xmin, xmax, a, b);
      line_x[1] = X_LOG(X_LIN(x_value + tick, scale_options, xmin, xmax, a, b), scale_options, xmin, xmax, a, b);

      if (!is_horizontal)
        {
          double tmp1, tmp2;
          tmp1 = line_x[0], tmp2 = line_x[1];
          line_x[0] = line_y[0], line_x[1] = line_y[1];
          line_y[0] = tmp1, line_y[1] = tmp2;
        }

      if (color_errorbar >= 0)
        {
          line_y[0] = e_upwards != FLT_MAX ? e_upwards : y_vec[i];
          line_y[1] = e_downwards != FLT_MAX ? e_downwards : y_vec[i];
          if (del != del_values::update_without_default && del != del_values::update_with_default)
            {
              errorbar = global_render->createErrorbar(x_value, line_y[0], line_y[1], color_errorbar);
              errorbar->setAttribute("_child_id", child_id++);
              element->append(errorbar);
            }
          else
            {
              errorbar = element->querySelectors("[_child_id=" + std::to_string(child_id++) + "]");
              if (errorbar != nullptr)
                global_render->createErrorbar(x_value, line_y[0], line_y[1], color_errorbar, errorbar);
            }

          if (errorbar != nullptr)
            {
              if (e_upwards != FLT_MAX)
                {
                  errorbar->setAttribute("e_upwards", e_upwards);
                  errorbar->setAttribute("color_upwardscap", color_upwardscap);
                }
              if (e_downwards != FLT_MAX)
                {
                  errorbar->setAttribute("e_downwards", e_downwards);
                  errorbar->setAttribute("color_downwardscap", color_downwardscap);
                }
              if (e_downwards != FLT_MAX || e_upwards != FLT_MAX)
                {
                  errorbar->setAttribute("scap_xmin", line_x[0]);
                  errorbar->setAttribute("scap_xmax", line_x[1]);
                }
            }
        }
    }
  gr_restorestate();
}

static void processErrorbar(const std::shared_ptr<GRM::Element> &element, const std::shared_ptr<GRM::Context> &context)
{
  double scap_xmin, scap_xmax, e_upwards = FLT_MAX, e_downwards = FLT_MAX;
  double errorbar_x, errorbar_ymin, errorbar_ymax;
  int color_upwardscap, color_downwardscap, color_errorbar;
  std::shared_ptr<GRM::Element> line;
  del_values del = del_values::update_without_default;
  int child_id = 0;

  /* clear old lines */
  del = del_values(static_cast<int>(element->getAttribute("_delete_children")));
  clearOldChildren(&del, element);

  errorbar_x = static_cast<double>(element->getAttribute("errorbar_x"));
  errorbar_ymin = static_cast<double>(element->getAttribute("errorbar_ymin"));
  errorbar_ymax = static_cast<double>(element->getAttribute("errorbar_ymax"));
  color_errorbar = static_cast<int>(element->getAttribute("color_errorbar"));

  if (element->hasAttribute("scap_xmin")) scap_xmin = static_cast<double>(element->getAttribute("scap_xmin"));
  if (element->hasAttribute("scap_xmax")) scap_xmax = static_cast<double>(element->getAttribute("scap_xmax"));
  if (element->hasAttribute("e_upwards")) e_upwards = static_cast<double>(element->getAttribute("e_upwards"));
  if (element->hasAttribute("e_downwards")) e_downwards = static_cast<double>(element->getAttribute("e_downwards"));
  if (element->hasAttribute("color_upwardscap"))
    color_upwardscap = static_cast<int>(element->getAttribute("color_upwardscap"));
  if (element->hasAttribute("color_downwardscap"))
    color_downwardscap = static_cast<int>(element->getAttribute("color_downwardscap"));

  if (e_upwards != FLT_MAX && color_upwardscap >= 0)
    {
      if (del != del_values::update_without_default && del != del_values::update_with_default)
        {
          line = global_render->createPolyline(scap_xmin, scap_xmax, e_upwards, e_upwards, 0, 0.0, color_upwardscap);
          line->setAttribute("_child_id", child_id++);
          element->append(line);
        }
      else
        {
          line = element->querySelectors("[_child_id=" + std::to_string(child_id++) + "]");
          if (line != nullptr)
            global_render->createPolyline(scap_xmin, scap_xmax, e_upwards, e_upwards, 0, 0.0, color_upwardscap, line);
        }
    }

  if (e_downwards != FLT_MAX && color_downwardscap >= 0)
    {
      if (del != del_values::update_without_default && del != del_values::update_with_default)
        {
          line =
              global_render->createPolyline(scap_xmin, scap_xmax, e_downwards, e_downwards, 0, 0.0, color_downwardscap);
          line->setAttribute("_child_id", child_id++);
          element->append(line);
        }
      else
        {
          line = element->querySelectors("[_child_id=" + std::to_string(child_id++) + "]");
          if (line != nullptr)
            global_render->createPolyline(scap_xmin, scap_xmax, e_downwards, e_downwards, 0, 0.0, color_downwardscap,
                                          line);
        }
    }

  if (color_errorbar >= 0)
    {
      if (del != del_values::update_without_default && del != del_values::update_with_default)
        {
          line = global_render->createPolyline(errorbar_x, errorbar_x, errorbar_ymin, errorbar_ymax, 0, 0.0,
                                               color_errorbar);
          line->setAttribute("_child_id", child_id++);
          element->append(line);
        }
      else
        {
          line = element->querySelectors("[_child_id=" + std::to_string(child_id++) + "]");
          if (line != nullptr)
            global_render->createPolyline(errorbar_x, errorbar_x, errorbar_ymin, errorbar_ymax, 0, 0.0, color_errorbar,
                                          line);
        }
    }
}

static void processIsosurface(const std::shared_ptr<GRM::Element> &element,
                              const std::shared_ptr<GRM::Context> &context)
{
  std::vector<double> c_vec, temp_colors;
  unsigned int i, c_length, dims;
  int strides[3];
  double c_min, c_max, isovalue;
  float foreground_colors[3];

  if (!element->hasAttribute("c")) throw NotFoundError("Isosurface series is missing required attribute c-data.\n");
  auto c = static_cast<std::string>(element->getAttribute("c"));
  c_vec = GRM::get<std::vector<double>>((*context)[c]);
  c_length = c_vec.size();

  if (!element->hasAttribute("c_dims"))
    throw NotFoundError("Isosurface series is missing required attribute c_dims.\n");
  auto c_dims = static_cast<std::string>(element->getAttribute("c_dims"));
  auto shape_vec = GRM::get<std::vector<int>>((*context)[c_dims]);
  dims = shape_vec.size();

  if (dims != 3) throw std::length_error("For isosurface series the size of c_dims has to be 3.\n");
  if (shape_vec[0] * shape_vec[1] * shape_vec[2] != c_length)
    throw std::length_error("For isosurface series shape[0] * shape[1] * shape[2] must be c_length.\n");
  if (c_length <= 0) throw NotFoundError("For isosurface series the size of c has to be greater than 0.\n");

  isovalue = 0.5;
  foreground_colors[0] = 0.0;
  foreground_colors[1] = 0.5;
  foreground_colors[2] = 0.8;
  if (element->hasAttribute("isovalue")) isovalue = static_cast<double>(element->getAttribute("isovalue"));
  element->setAttribute("isovalue", isovalue);
  /*
   * We need to convert the double values to floats, as GR3 expects floats, but an argument can only contain
   * doubles.
   */
  if (element->hasAttribute("foreground_color"))
    {
      auto temp_c = static_cast<std::string>(element->getAttribute("foreground_color"));
      temp_colors = GRM::get<std::vector<double>>((*context)[temp_c]);
      i = temp_colors.size();
      if (i != 3) throw std::length_error("For isosurface series the foreground colors must have size 3.\n");
      while (i-- > 0)
        {
          foreground_colors[i] = (float)temp_colors[i];
        }
    }
  logger((stderr, "Colors; %f %f %f\n", foreground_colors[0], foreground_colors[1], foreground_colors[2]));

  /* Check if any value is finite in array, also calculation of real min and max */
  c_min = c_max = c_vec[0];
  for (i = 0; i < c_length; ++i)
    {
      if (std::isfinite(c_vec[i]))
        {
          if (grm_isnan(c_min) || c_min > c_vec[i])
            {
              c_min = c_vec[i];
            }
          if (grm_isnan(c_max) || c_max < c_vec[i])
            {
              c_max = c_vec[i];
            }
        }
    }
  if (c_min == c_max || !std::isfinite(c_min) || !std::isfinite(c_max))
    throw NotFoundError("For isosurface series the given c-data isn't enough.\n");

  logger((stderr, "c_min %lf c_max %lf isovalue %lf\n ", c_min, c_max, isovalue));
  std::vector<float> conv_data(c_vec.begin(), c_vec.end());

  strides[0] = shape_vec[1] * shape_vec[2];
  strides[1] = shape_vec[2];
  strides[2] = 1;

  global_render->setGR3LightParameters(element, 0.2, 0.8, 0.7, 128);

  float ambient = static_cast<double>(element->getAttribute("ambient"));
  float diffuse = static_cast<double>(element->getAttribute("diffuse"));
  float specular = static_cast<double>(element->getAttribute("specular"));
  float specular_power = static_cast<double>(element->getAttribute("specular_power"));

  float *data = &(conv_data[0]);
  float light_parameters[4];

  gr3_clear();

  /* Save and restore original light parameters */
  gr3_getlightparameters(&light_parameters[0], &light_parameters[1], &light_parameters[2], &light_parameters[3]);
  gr3_setlightparameters(ambient, diffuse, specular, specular_power);

  gr3_isosurface(shape_vec[0], shape_vec[1], shape_vec[2], data, (float)isovalue, foreground_colors, strides);

  gr3_setlightparameters(light_parameters[0], light_parameters[1], light_parameters[2], light_parameters[3]);
}

static void processLegend(const std::shared_ptr<GRM::Element> &element, const std::shared_ptr<GRM::Context> &context)
{
  double viewport[4];
  double px, py, w, h;
  double tbx[4], tby[4];
  std::shared_ptr<GRM::Render> render;
  std::string labels_key = static_cast<std::string>(element->getAttribute("labels"));
  auto labels = GRM::get<std::vector<std::string>>((*context)[labels_key]);
  del_values del = del_values::update_without_default;
  int child_id = 0;

  render = std::dynamic_pointer_cast<GRM::Render>(element->ownerDocument());
  if (!render)
    {
      throw NotFoundError("No render-document found for element\n");
    }

  gr_inqviewport(&viewport[0], &viewport[1], &viewport[2], &viewport[3]);

  if (static_cast<std::string>(element->parentElement()->getAttribute("kind")) != "pie")
    {
      int location = PLOT_DEFAULT_LOCATION;
      double legend_symbol_x[2], legend_symbol_y[2];
      int i;
      std::shared_ptr<GRM::Element> fr, dr;

      gr_savestate();

      auto specs_key = static_cast<std::string>(element->getAttribute("specs"));
      std::vector<std::string> specs = GRM::get<std::vector<std::string>>((*context)[specs_key]);
      if (element->hasAttribute("location"))
        {
          location = static_cast<int>(element->getAttribute("location"));
        }
      else
        {
          element->setAttribute("location", location);
        }

      legend_size(labels, &w, &h);

      if (int_equals_any(location, 3, 11, 12, 13))
        {
          px = viewport[1] + 0.11;
        }
      else if (int_equals_any(location, 3, 8, 9, 10))
        {
          px = 0.5 * (viewport[0] + viewport[1] - w + 0.05);
        }
      else if (int_equals_any(location, 3, 2, 3, 6))
        {
          px = viewport[0] + 0.11;
        }
      else
        {
          px = viewport[1] - 0.05 - w;
        }
      if (int_equals_any(location, 5, 5, 6, 7, 10, 12))
        {
          py = 0.5 * (viewport[2] + viewport[3] + h) - 0.03;
        }
      else if (location == 13)
        {
          py = viewport[2] + h;
        }
      else if (int_equals_any(location, 3, 3, 4, 8))
        {
          py = viewport[2] + h + 0.03;
        }
      else if (location == 11)
        {
          py = viewport[3] - 0.03;
        }
      else
        {
          py = viewport[3] - 0.06;
        }

      del = del_values(static_cast<int>(element->getAttribute("_delete_children")));

      /* get the amount of series which should be displayed inside the legend */
      int legend_elems = 0;
      for (const auto &child : element->parentElement()->children())
        {
          if (child->localName() != "series_line" && child->localName() != "series_scatter") continue;
          for (const auto &childchild : child->children())
            {
              if (childchild->localName() != "polyline" && childchild->localName() != "polymarker") continue;
              legend_elems += 1;
            }
        }
      if (element->hasAttribute("_legend_elems"))
        {
          /* the amount has changed - all legend children have to recreated cause its unknown which is new or gone */
          if (static_cast<int>(element->getAttribute("_legend_elems")) != legend_elems)
            {
              del = (del == del_values::recreate_all_children) ? del_values::recreate_all_children
                                                               : del_values::recreate_own_children;
              element->setAttribute("_legend_elems", legend_elems);
            }
        }
      else
        {
          element->setAttribute("_legend_elems", legend_elems);
        }

      /* clear old child nodes */
      clearOldChildren(&del, element);

      gr_selntran(1);

      render->setSelntran(element, 0);
      render->setScale(element, 0);

      if (del != del_values::update_without_default && del != del_values::update_with_default)
        {
          fr = render->createFillRect(px - 0.08, px + w + 0.02, py + 0.03, py - h);
          fr->setAttribute("_child_id", child_id++);
          element->append(fr);
        }
      else
        {
          fr = element->querySelectors("[_child_id=" + std::to_string(child_id++) + "]");
          if (fr != nullptr) render->createFillRect(px - 0.08, px + w + 0.02, py + 0.03, py - h, 0, 0, -1, fr);
        }

      render->setFillIntStyle(element, GKS_K_INTSTYLE_SOLID);
      render->setFillColorInd(element, 0);

      if (del != del_values::update_without_default && del != del_values::update_with_default)
        {
          dr = render->createDrawRect(px - 0.08, px + w + 0.02, py + 0.03, py - h);
          dr->setAttribute("_child_id", child_id++);
          element->append(dr);
        }
      else
        {
          dr = element->querySelectors("[_child_id=" + std::to_string(child_id++) + "]");
          if (dr != nullptr) render->createDrawRect(px - 0.08, px + w + 0.02, py + 0.03, py - h, dr);
        }

      if (dr != nullptr && del != del_values::update_without_default)
        {
          render->setLineType(dr, GKS_K_INTSTYLE_SOLID);
          render->setLineColorInd(dr, 1);
          render->setLineWidth(dr, 1);
        }

      i = 0;
      render->setLineSpec(element, const_cast<char *>(" "));

      int spec_i = 0;
      for (const auto &child : element->parentElement()->children())
        {
          int mask;
          double dy;

          if (child->localName() != "series_line" && child->localName() != "series_scatter") continue;

          if (i <= labels.size())
            {
              gr_inqtext(0, 0, labels[i].data(), tbx, tby);
              dy = grm_max((tby[2] - tby[0]) - 0.03, 0);
              py -= 0.5 * dy;
            }

          gr_savestate();
          mask = gr_uselinespec(specs[spec_i].data());
          gr_restorestate();

          if (int_equals_any(mask, 5, 0, 1, 3, 4, 5))
            {
              legend_symbol_x[0] = px - 0.07;
              legend_symbol_x[1] = px - 0.01;
              legend_symbol_y[0] = py;
              legend_symbol_y[1] = py;
              for (const auto &childchild : child->children())
                {
                  std::shared_ptr<GRM::Element> pl;
                  if (childchild->localName() == "polyline")
                    {
                      if (del != del_values::update_without_default && del != del_values::update_with_default)
                        {
                          pl = render->createPolyline(legend_symbol_x[0], legend_symbol_x[1], legend_symbol_y[0],
                                                      legend_symbol_y[1]);
                          pl->setAttribute("_child_id", child_id++);
                          element->append(pl);
                        }
                      else
                        {
                          pl = element->querySelectors("[_child_id=" + std::to_string(child_id++) + "]");
                          if (pl != nullptr)
                            render->createPolyline(legend_symbol_x[0], legend_symbol_x[1], legend_symbol_y[0],
                                                   legend_symbol_y[1], 0, 0.0, 0, pl);
                        }
                      if (pl != nullptr)
                        {
                          render->setLineSpec(pl, specs[spec_i]);
                          if (childchild->hasAttribute("linecolorind"))
                            {
                              render->setLineColorInd(pl, static_cast<int>(childchild->getAttribute("linecolorind")));
                            }
                          else
                            {
                              render->setLineColorInd(pl, static_cast<int>(child->getAttribute("linecolorind")));
                            }
                        }
                    }
                  else if (childchild->localName() == "polymarker")
                    {
                      int markertype;
                      if (childchild->hasAttribute("markertype"))
                        {
                          markertype = static_cast<int>(childchild->getAttribute("markertype"));
                        }
                      else
                        {
                          markertype = static_cast<int>(child->getAttribute("markertype"));
                        }
                      if (del != del_values::update_without_default && del != del_values::update_with_default)
                        {
                          pl = render->createPolymarker(legend_symbol_x[0] + 0.02, legend_symbol_y[0], markertype);
                          pl->setAttribute("_child_id", child_id++);
                          element->append(pl);
                        }
                      else
                        {
                          pl = element->querySelectors("[_child_id=" + std::to_string(child_id++) + "]");
                          if (pl != nullptr)
                            render->createPolymarker(legend_symbol_x[0] + 0.02, legend_symbol_y[0], markertype, 0.0, 0,
                                                     pl);
                        }
                      if (pl != nullptr)
                        {
                          render->setMarkerColorInd(pl, (child->hasAttribute("markercolorind")
                                                             ? static_cast<int>(child->getAttribute("markercolorind"))
                                                             : 989));
                          processMarkerColorInd(pl);
                        }
                    }
                }
            }
          if (mask & 2)
            {
              legend_symbol_x[0] = px - 0.06;
              legend_symbol_x[1] = px - 0.02;
              legend_symbol_y[0] = py;
              legend_symbol_y[1] = py;
              for (const auto &childchild : child->children())
                {
                  std::shared_ptr<GRM::Element> pl;
                  if (childchild->localName() == "polyline")
                    {
                      if (del != del_values::update_without_default && del != del_values::update_with_default)
                        {
                          pl = render->createPolyline(legend_symbol_x[0], legend_symbol_x[1], legend_symbol_y[0],
                                                      legend_symbol_y[1]);
                          pl->setAttribute("_child_id", child_id++);
                          element->append(pl);
                        }
                      else
                        {
                          pl = element->querySelectors("[_child_id=" + std::to_string(child_id++) + "]");
                          if (pl != nullptr)
                            render->createPolyline(legend_symbol_x[0], legend_symbol_x[1], legend_symbol_y[0],
                                                   legend_symbol_y[1], 0, 0.0, 0, pl);
                        }
                      if (pl != nullptr)
                        {
                          render->setLineSpec(pl, specs[spec_i]);
                          if (childchild->hasAttribute("linecolorind"))
                            {
                              render->setLineColorInd(pl, static_cast<int>(childchild->getAttribute("linecolorind")));
                            }
                          else
                            {
                              render->setLineColorInd(pl, static_cast<int>(child->getAttribute("linecolorind")));
                            }
                        }
                    }
                  else if (childchild->localName() == "polymarker")
                    {
                      int markertype;
                      if (childchild->hasAttribute("markertype"))
                        {
                          markertype = static_cast<int>(childchild->getAttribute("markertype"));
                        }
                      else
                        {
                          markertype = static_cast<int>(child->getAttribute("markertype"));
                        }
                      if (del != del_values::update_without_default && del != del_values::update_with_default)
                        {
                          pl = render->createPolymarker(legend_symbol_x[0] + 0.02, legend_symbol_y[0], markertype);
                          pl->setAttribute("_child_id", child_id++);
                          element->append(pl);
                        }
                      else
                        {
                          pl = element->querySelectors("[_child_id=" + std::to_string(child_id++) + "]");
                          if (pl != nullptr)
                            render->createPolymarker(legend_symbol_x[0] + 0.02, legend_symbol_y[0], markertype, 0.0, 0,
                                                     pl);
                        }
                      if (pl != nullptr)
                        {
                          render->setMarkerColorInd(pl, (child->hasAttribute("markercolorind")
                                                             ? static_cast<int>(child->getAttribute("markercolorind"))
                                                             : 989));
                          processMarkerColorInd(pl);
                        }
                    }
                }
            }
          if (i < labels.size())
            {
              std::shared_ptr<GRM::Element> tx;
              if (del != del_values::update_without_default && del != del_values::update_with_default)
                {
                  tx = render->createText(px, py, labels[i].data());
                  tx->setAttribute("_child_id", child_id++);
                  element->append(tx);
                }
              else
                {
                  tx = element->querySelectors("[_child_id=" + std::to_string(child_id++) + "]");
                  if (tx != nullptr) render->createText(px, py, labels[i].data(), CoordinateSpace::NDC, tx);
                }
              if (tx != nullptr && del != del_values::update_without_default)
                {
                  render->setTextAlign(tx, GKS_K_TEXT_HALIGN_LEFT, GKS_K_TEXT_VALIGN_HALF);
                }
              py -= 0.5 * dy;
              i += 1;
            }
          py -= 0.03;
          spec_i += 1;
        }
      gr_restorestate();

      processLineSpec(element);
    }
  else
    {
      unsigned int num_labels = labels.size();
      std::shared_ptr<GRM::Element> fr, dr;
      int label_child_id = 0;

      /* clear child nodes */
      del = del_values(static_cast<int>(element->getAttribute("_delete_children")));
      clearOldChildren(&del, element);

      gr_selntran(1);

      render->setSelntran(element, 0);
      render->setScale(element, 0);
      w = 0;
      h = 0;
      for (auto current_label : labels)
        {
          gr_inqtext(0, 0, current_label.data(), tbx, tby);
          w += tbx[2] - tbx[0];
          h = grm_max(h, tby[2] - tby[0]);
        }
      w += num_labels * 0.03 + (num_labels - 1) * 0.02;

      px = 0.5 * (viewport[0] + viewport[1] - w);
      py = viewport[2] - 0.75 * h;

      if (del != del_values::update_without_default && del != del_values::update_with_default)
        {
          fr = render->createFillRect(px - 0.02, px + w + 0.02, py - 0.5 * h - 0.02, py + 0.5 * h + 0.02);
          fr->setAttribute("_child_id", child_id++);
          element->append(fr);
        }
      else
        {
          fr = element->querySelectors("[_child_id=" + std::to_string(child_id++) + "]");
          if (fr != nullptr)
            render->createFillRect(px - 0.02, px + w + 0.02, py - 0.5 * h - 0.02, py + 0.5 * h + 0.02, 0, 0, -1, fr);
        }

      render->setFillIntStyle(element, GKS_K_INTSTYLE_SOLID);
      render->setFillColorInd(element, 0);

      if (del != del_values::update_without_default && del != del_values::update_with_default)
        {
          dr = render->createDrawRect(px - 0.02, px + w + 0.02, py - 0.5 * h - 0.02, py + 0.5 * h + 0.02);
          dr->setAttribute("_child_id", child_id++);
          element->append(dr);
        }
      else
        {
          dr = element->querySelectors("[_child_id=" + std::to_string(child_id++) + "]");
          if (dr != nullptr)
            render->createDrawRect(px - 0.02, px + w + 0.02, py - 0.5 * h - 0.02, py + 0.5 * h + 0.02, dr);
        }

      render->setLineType(element, GKS_K_INTSTYLE_SOLID);
      render->setLineColorInd(element, 1);
      render->setLineWidth(element, 1);

      auto subsubGroup = element->querySelectors("labels_group");
      if (subsubGroup == nullptr)
        {
          subsubGroup = render->createElement("labels_group");
          element->append(subsubGroup);
        }
      render->setTextAlign(subsubGroup, GKS_K_TEXT_HALIGN_LEFT, GKS_K_TEXT_VALIGN_HALF);

      for (auto &current_label : labels)
        {
          std::shared_ptr<GRM::Element> labelGroup;
          if (del != del_values::update_without_default && del != del_values::update_with_default)
            {
              labelGroup = render->createElement("label");
              labelGroup->setAttribute("_child_id", label_child_id++);
              subsubGroup->append(labelGroup);
            }
          else
            {
              auto selections = subsubGroup->querySelectorsAll("[_child_id=" + std::to_string(label_child_id++) + "]");
              for (const auto &sel : selections)
                {
                  if (sel->localName() == "label")
                    {
                      labelGroup = sel;
                      break;
                    }
                }
            }
          if (labelGroup != nullptr)
            {
              std::shared_ptr<GRM::Element> fr, dr, text;
              if (del != del_values::update_without_default && del != del_values::update_with_default)
                {
                  fr = render->createFillRect(px, px + 0.02, py - 0.01, py + 0.01);
                  fr->setAttribute("_child_id", 0);
                  labelGroup->append(fr);
                }
              else
                {
                  auto selections = labelGroup->querySelectorsAll("[_child_id=0]");
                  for (const auto &sel : selections)
                    {
                      if (sel->localName() == "fillrect")
                        {
                          fr = sel;
                          break;
                        }
                    }
                  if (fr != nullptr) render->createFillRect(px, px + 0.02, py - 0.01, py + 0.01, 0, 0, -1, fr);
                }
              if (fr != nullptr)
                {
                  for (const auto &child : element->parentElement()->children())
                    {
                      if (child->localName() == "series_pie")
                        {
                          std::shared_ptr<GRM::Element> pie_segment;
                          auto pie_childs =
                              child->querySelectorsAll("[_child_id=" + std::to_string(label_child_id - 1) + "]");
                          for (const auto &segments : pie_childs)
                            {
                              if (segments->localName() == "pie_segment")
                                {
                                  pie_segment = segments;
                                  break;
                                }
                            }
                          if (pie_segment != nullptr)
                            {
                              int color_ind = static_cast<int>(pie_segment->getAttribute("fillcolorind"));
                              auto colorrep = static_cast<std::string>(
                                  pie_segment->getAttribute("colorrep." + std::to_string(color_ind)));
                              fr->setAttribute("fillcolorind", color_ind);
                              if (!colorrep.empty())
                                fr->setAttribute("colorrep." + std::to_string(color_ind), colorrep);
                            }
                          break;
                        }
                    }
                }

              if (del != del_values::update_without_default && del != del_values::update_with_default)
                {
                  dr = render->createDrawRect(px, px + 0.02, py - 0.01, py + 0.01);
                  dr->setAttribute("_child_id", 1);
                  labelGroup->append(dr);
                }
              else
                {
                  auto selections = labelGroup->querySelectorsAll("[_child_id=1]");
                  for (const auto &sel : selections)
                    {
                      if (sel->localName() == "drawrect")
                        {
                          dr = sel;
                          break;
                        }
                    }
                  if (dr != nullptr) render->createDrawRect(px, px + 0.02, py - 0.01, py + 0.01, dr);
                }
              if (del != del_values::update_without_default && del != del_values::update_with_default)
                {
                  text = render->createText(px + 0.03, py, current_label);
                  text->setAttribute("_child_id", 2);
                  labelGroup->append(text);
                }
              else
                {
                  auto selections = labelGroup->querySelectorsAll("[_child_id=2]");
                  for (const auto &sel : selections)
                    {
                      if (sel->localName() == "text")
                        {
                          text = sel;
                          break;
                        }
                    }
                  if (text != nullptr) render->createText(px + 0.03, py, current_label, CoordinateSpace::NDC, text);
                }

              gr_inqtext(0, 0, current_label.data(), tbx, tby);
              px += tbx[2] - tbx[0] + 0.05;
            }
        }

      processLineColorInd(element);
      processLineWidth(element);
      processLineType(element);
    }
  processSelntran(element);
  GRM::Render::processScale(element);
  processFillIntStyle(element);
  processFillColorInd(element);
}

static void processPolarAxes(const std::shared_ptr<GRM::Element> &element, const std::shared_ptr<GRM::Context> &context)
{
  double viewport[4], vp[4];
  double diag;
  double charheight;
  double r_min, r_max;
  double tick;
  double x[2], y[2];
  int i, n;
  double alpha;
  char text_buffer[PLOT_POLAR_AXES_TEXT_BUFFER];
  std::string kind;
  int angle_ticks, rings;
  int phiflip = 0;
  double interval;
  std::string title;
  std::string norm;
  del_values del = del_values::update_without_default;
  int child_id = 0;

  std::shared_ptr<GRM::Render> render;

  gr_inqviewport(&viewport[0], &viewport[1], &viewport[2], &viewport[3]);
  auto subplot = getSubplotElement(element);

  render = std::dynamic_pointer_cast<GRM::Render>(element->ownerDocument());
  if (!render)
    {
      throw NotFoundError("No render-document for element found\n");
    }

  auto subplotElement = getSubplotElement(element);
  vp[0] = static_cast<double>(subplotElement->getAttribute("vp_xmin"));
  vp[1] = static_cast<double>(subplotElement->getAttribute("vp_xmax"));
  vp[2] = static_cast<double>(subplotElement->getAttribute("vp_ymin"));
  vp[3] = static_cast<double>(subplotElement->getAttribute("vp_ymax"));

  diag = std::sqrt((viewport[1] - viewport[0]) * (viewport[1] - viewport[0]) +
                   (viewport[3] - viewport[2]) * (viewport[3] - viewport[2]));
  charheight = grm_max(0.018 * diag, 0.012);

  r_min = static_cast<double>(subplot->getAttribute("_ylim_min"));
  r_max = static_cast<double>(subplot->getAttribute("_ylim_max"));

  angle_ticks = static_cast<int>(element->getAttribute("angle_ticks"));

  kind = static_cast<std::string>(subplotElement->getAttribute("kind"));

  render->setLineType(element, GKS_K_LINETYPE_SOLID);

  if (kind == "polar_histogram")
    {
      auto max = static_cast<double>(subplotElement->getAttribute("r_max"));

      // check if rings are given
      rings = -1;
      norm = static_cast<std::string>(element->getAttribute("norm"));

      tick = auto_tick_rings_polar(max, rings, norm);
      subplotElement->setAttribute("tick", tick);
      max = tick * rings;
      subplotElement->setAttribute("r_max", max);
      subplotElement->setAttribute("rings", rings);

      r_min = 0.0;
    }
  else
    {
      rings = grm_max(4, (int)(r_max - r_min));
      element->setAttribute("rings", rings);

      if (element->hasAttribute("tick"))
        {
          tick = static_cast<double>(element->getAttribute("tick"));
        }
      else
        {
          tick = auto_tick(r_min, r_max);
        }
    }

  /* clear old polar_axes_elements */
  del = del_values(static_cast<int>(element->getAttribute("_delete_children")));
  clearOldChildren(&del, element);

  n = rings;
  phiflip = static_cast<int>(subplotElement->getAttribute("phiflip"));

  // Draw rings
  for (i = 0; i <= n; i++)
    {
      std::shared_ptr<GRM::Element> arc;
      double r = 1.0 / n * i;
      if (del != del_values::update_without_default && del != del_values::update_with_default)
        {
          arc = render->createDrawArc(-r, r, -r, r, 0, 360);
          arc->setAttribute("_child_id", child_id++);
          element->append(arc);
        }
      else
        {
          arc = element->querySelectors("[_child_id=" + std::to_string(child_id++) + "]");
          if (arc != nullptr) render->createDrawArc(-r, r, -r, r, 0, 360, arc);
        }
      if (arc != nullptr) arc->setAttribute("name", "polar_axes");
      if (i % 2 == 0)
        {
          if (i > 0)
            {
              if (arc != nullptr && del != del_values::update_without_default) render->setLineColorInd(arc, 88);
            }
        }
      else
        {
          if (arc != nullptr && del != del_values::update_without_default) render->setLineColorInd(arc, 90);
        }
    }

  // Draw sectorlines
  interval = 360.0 / angle_ticks;
  for (alpha = 0.0; alpha < 360; alpha += interval)
    {
      std::shared_ptr<GRM::Element> line, text;
      x[0] = std::cos(alpha * M_PI / 180.0);
      y[0] = std::sin(alpha * M_PI / 180.0);
      x[1] = 0.0;
      y[1] = 0.0;

      if (del != del_values::update_without_default && del != del_values::update_with_default)
        {
          line = render->createPolyline(x[0], x[1], y[0], y[1]);
          line->setAttribute("_child_id", child_id++);
          element->append(line);
        }
      else
        {
          line = element->querySelectors("[_child_id=" + std::to_string(child_id++) + "]");
          if (line != nullptr) render->createPolyline(x[0], x[1], y[0], y[1], 0, 0.0, 0, line);
        }
      if (line != nullptr && del != del_values::update_without_default) render->setLineColorInd(line, 88);
      if (line != nullptr) line->setAttribute("name", "polar_axes");

      x[0] *= 1.1;
      y[0] *= 1.1;
      if (phiflip == 0)
        {
          snprintf(text_buffer, PLOT_POLAR_AXES_TEXT_BUFFER, "%d\xc2\xb0", (int)grm_round(alpha));
        }
      else
        {
          if (alpha == 0.0)
            snprintf(text_buffer, PLOT_POLAR_AXES_TEXT_BUFFER, "%d\xc2\xb0", 0);
          else
            snprintf(text_buffer, PLOT_POLAR_AXES_TEXT_BUFFER, "%d\xc2\xb0", 330 - (int)grm_round(alpha - interval));
        }
      if (del != del_values::update_without_default && del != del_values::update_with_default)
        {
          text = render->createText(x[0], y[0], text_buffer, CoordinateSpace::WC);
          text->setAttribute("_child_id", child_id++);
          element->append(text);
        }
      else
        {
          text = element->querySelectors("[_child_id=" + std::to_string(child_id++) + "]");
          if (text != nullptr) render->createText(x[0], y[0], text_buffer, CoordinateSpace::WC, text);
        }
      if (text != nullptr) text->setAttribute("name", "polar_axes");
      if (text != nullptr && del != del_values::update_without_default)
        render->setTextAlign(text, GKS_K_TEXT_HALIGN_CENTER, GKS_K_TEXT_VALIGN_HALF);
    }

  // Draw Text
  render->setTextAlign(element, GKS_K_TEXT_HALIGN_LEFT, GKS_K_TEXT_VALIGN_HALF);
  std::shared_ptr<GRM::Element> axesTextGroup = element->querySelectors("axes_text_group");
  if (axesTextGroup == nullptr)
    {
      axesTextGroup = render->createElement("axes_text_group");
      element->append(axesTextGroup);
    }
  render->setCharHeight(axesTextGroup, charheight);

  for (i = 0; i <= n; i++)
    {
      std::shared_ptr<GRM::Element> text;
      double r = 1.0 / n * i;
      if (i % 2 == 0 || i == n)
        {
          x[0] = 0.05;
          y[0] = r;
          snprintf(text_buffer, PLOT_POLAR_AXES_TEXT_BUFFER, "%.1lf", r_min + tick * i);

          if (del != del_values::update_without_default && del != del_values::update_with_default)
            {
              text = render->createText(x[0], y[0], text_buffer, CoordinateSpace::WC);
              text->setAttribute("_child_id", child_id++);
              axesTextGroup->append(text);
            }
          else
            {
              text = element->querySelectors("[_child_id=" + std::to_string(child_id++) + "]");
              if (text != nullptr) render->createText(x[0], y[0], text_buffer, CoordinateSpace::WC, text);
            }
        }
    }
  processCharHeight(element);
  processLineType(element);
  processTextAlign(element);
}

static void processDrawRect(const std::shared_ptr<GRM::Element> &element, const std::shared_ptr<GRM::Context> &context)
{
  /*!
   * Processing function for drawArc
   *
   * \param[in] element The GRM::Element that contains the attributes and data keys
   * \param[in] context The GRM::Context that contains the actual data
   */
  double xmin = static_cast<double>(element->getAttribute("xmin"));
  double xmax = static_cast<double>(element->getAttribute("xmax"));
  double ymin = static_cast<double>(element->getAttribute("ymin"));
  double ymax = static_cast<double>(element->getAttribute("ymax"));
  if (redrawws) gr_drawrect(xmin, xmax, ymin, ymax);
}

static void processFillArc(const std::shared_ptr<GRM::Element> &element, const std::shared_ptr<GRM::Context> &context)
{
  /*!
   * Processing function for drawArc
   *
   * \param[in] element The GRM::Element that contains the attributes and data keys
   * \param[in] context The GRM::Context that contains the actual data
   */
  double xmin = static_cast<double>(element->getAttribute("xmin"));
  double xmax = static_cast<double>(element->getAttribute("xmax"));
  double ymin = static_cast<double>(element->getAttribute("ymin"));
  double ymax = static_cast<double>(element->getAttribute("ymax"));
  double a1 = static_cast<double>(element->getAttribute("a1"));
  double a2 = static_cast<double>(element->getAttribute("a2"));
  if (redrawws) gr_fillarc(xmin, xmax, ymin, ymax, a1, a2);
}

static void processFillRect(const std::shared_ptr<GRM::Element> &element, const std::shared_ptr<GRM::Context> &context)
{
  /*!
   * Processing function for drawArc
   *
   * \param[in] element The GRM::Element that contains the attributes and data keys
   * \param[in] context The GRM::Context that contains the actual data
   */
  double xmin = static_cast<double>(element->getAttribute("xmin"));
  double xmax = static_cast<double>(element->getAttribute("xmax"));
  double ymin = static_cast<double>(element->getAttribute("ymin"));
  double ymax = static_cast<double>(element->getAttribute("ymax"));
  if (redrawws) gr_fillrect(xmin, xmax, ymin, ymax);
}

static void processFillArea(const std::shared_ptr<GRM::Element> &element, const std::shared_ptr<GRM::Context> &context)
{
  /*!
   * Processing function for fillArea
   *
   * \param[in] element The GRM::Element that contains the attributes and data keys
   * \param[in] context The GRM::Context that contains the actual data
   */
  auto x = static_cast<std::string>(element->getAttribute("x"));
  auto y = static_cast<std::string>(element->getAttribute("y"));

  std::vector<double> x_vec = GRM::get<std::vector<double>>((*context)[x]);
  std::vector<double> y_vec = GRM::get<std::vector<double>>((*context)[y]);

  int n = std::min<int>(x_vec.size(), y_vec.size());

  if (redrawws) gr_fillarea(n, (double *)&(x_vec[0]), (double *)&(y_vec[0]));
}

static void processGr3Clear(const std::shared_ptr<GRM::Element> &element, const std::shared_ptr<GRM::Context> &context)
{
  gr3_clear();
}

static void processGr3DeleteMesh(const std::shared_ptr<GRM::Element> &element,
                                 const std::shared_ptr<GRM::Context> &context)
{
  int mesh = static_cast<int>(element->getAttribute("mesh"));
  gr3_deletemesh(mesh);
}

static void processGr3DrawImage(const std::shared_ptr<GRM::Element> &element,
                                const std::shared_ptr<GRM::Context> &context)
{
  double xmin, xmax, ymin, ymax;
  int width, height, drawable_type;

  xmin = (double)element->getAttribute("xmin");
  xmax = (double)element->getAttribute("xmax");
  ymin = (double)element->getAttribute("ymin");
  ymax = (double)element->getAttribute("ymax");
  width = (int)element->getAttribute("width");
  height = (int)element->getAttribute("height");
  drawable_type = (int)element->getAttribute("drawable_type");

  logger((stderr, "gr3_drawimage returned %i\n", gr3_drawimage(xmin, xmax, ymin, ymax, width, height, drawable_type)));
}

static void processGr3DrawMesh(const std::shared_ptr<GRM::Element> &element,
                               const std::shared_ptr<GRM::Context> &context)
{
  /*!
   * Processing function for gr3_drawmesh
   *
   * \param[in] element The GRM::Element that contains the attributes and data keys
   * \param[in] context The GRM::Context that contains the actual data
   */

  int mesh = (int)element->getAttribute("mesh");
  int n = (int)element->getAttribute("n");

  auto positions = static_cast<std::string>(element->getAttribute("positions"));
  auto directions = static_cast<std::string>(element->getAttribute("directions"));
  auto ups = static_cast<std::string>(element->getAttribute("ups"));
  auto colors = static_cast<std::string>(element->getAttribute("colors"));
  auto scales = static_cast<std::string>(element->getAttribute("scales"));

  std::vector<double> positions_vec = GRM::get<std::vector<double>>((*context)[positions]);
  std::vector<double> directions_vec = GRM::get<std::vector<double>>((*context)[directions]);
  std::vector<double> ups_vec = GRM::get<std::vector<double>>((*context)[ups]);
  std::vector<double> colors_vec = GRM::get<std::vector<double>>((*context)[colors]);
  std::vector<double> scales_vec = GRM::get<std::vector<double>>((*context)[scales]);

  std::vector<float> pf_vec(positions_vec.begin(), positions_vec.end());
  std::vector<float> df_vec(directions_vec.begin(), directions_vec.end());
  std::vector<float> uf_vec(ups_vec.begin(), ups_vec.end());
  std::vector<float> cf_vec(colors_vec.begin(), colors_vec.end());
  std::vector<float> sf_vec(scales_vec.begin(), scales_vec.end());

  float *positions_p = &(pf_vec[0]);
  float *directions_p = &(df_vec[0]);
  float *ups_p = &(uf_vec[0]);
  float *colors_p = &(cf_vec[0]);
  float *scales_p = &(sf_vec[0]);

  gr3_drawmesh(mesh, n, positions_p, directions_p, ups_p, colors_p, scales_p);
}

static void processGrid(const std::shared_ptr<GRM::Element> &element, const std::shared_ptr<GRM::Context> &context)
{
  /*!
   * Processing function for grid
   *
   * \param[in] element The GRM::Element that contains the attributes and data keys
   * \param[in] context The GRM::Context that contains the actual data
   */
  double x_tick, y_tick, x_org, y_org;
  int x_major, y_major;
  std::string x_org_pos, y_org_pos;

  if (element->hasAttribute("x_org_pos"))
    {
      x_org_pos = static_cast<std::string>(element->getAttribute("x_org_pos"));
    }
  else
    {
      x_org_pos = "low";
    }
  if (element->hasAttribute("y_org_pos"))
    {
      y_org_pos = static_cast<std::string>(element->getAttribute("y_org_pos"));
    }
  else
    {
      y_org_pos = "low";
    }

  getAxesInformation(element, x_org_pos, y_org_pos, x_org, y_org, x_major, y_major, x_tick, y_tick);

  if (redrawws) gr_grid(x_tick, y_tick, x_org, y_org, abs(x_major), abs(y_major));
}

static void processGrid3d(const std::shared_ptr<GRM::Element> &element, const std::shared_ptr<GRM::Context> &context)
{
  /*!
   * Processing function for grid3d
   *
   * \param[in] element The GRM::Element that contains the attributes and data keys
   * \param[in] context The GRM::Context that contains the actual data
   */
  double x_tick, x_org;
  double y_tick, y_org;
  double z_tick, z_org;
  int x_major;
  int y_major;
  int z_major;
  std::string x_org_pos, y_org_pos, z_org_pos;
  if (element->hasAttribute("x_org_pos"))
    {
      x_org_pos = static_cast<std::string>(element->getAttribute("x_org_pos"));
    }
  else
    {
      x_org_pos = "low";
    }
  if (element->hasAttribute("y_org_pos"))
    {
      y_org_pos = static_cast<std::string>(element->getAttribute("y_org_pos"));
    }
  else
    {
      y_org_pos = "low";
    }
  if (element->hasAttribute("z_org_pos"))
    {
      z_org_pos = static_cast<std::string>(element->getAttribute("z_org_pos"));
    }
  else
    {
      z_org_pos = "low";
    }
  getAxes3dInformation(element, x_org_pos, y_org_pos, z_org_pos, x_org, y_org, z_org, x_major, y_major, z_major, x_tick,
                       y_tick, z_tick);

  if (redrawws) gr_grid3d(x_tick, y_tick, z_tick, x_org, y_org, z_org, abs(x_major), abs(y_major), abs(z_major));
}

static void processHeatmap(const std::shared_ptr<GRM::Element> &element, const std::shared_ptr<GRM::Context> &context)
{
  /*!
   * Processing function for heatmap
   *
   * \param[in] element The GRM::Element that contains the attributes and data keys
   * \param[in] context The GRM::Context that contains the actual data
   */

  int icmap[256], zlog = 0;
  unsigned int i, cols, rows, z_length;
  double x_min, x_max, y_min, y_max, z_min, z_max, c_min, c_max, zv;
  int is_uniform_heatmap;
  std::shared_ptr<GRM::Element> plot_parent;
  std::shared_ptr<GRM::Element> element_context = element;
  std::vector<int> data, rgba;
  std::vector<double> x_vec, y_vec, z_vec;
  del_values del = del_values::update_without_default;
  int child_id = 0;

  if (element->parentElement()->localName() == "plot")
    {
      plot_parent = element->parentElement();
    }
  else
    {
      element_context = element->parentElement();
      plot_parent = element->parentElement()->parentElement();
    }
  zlog = static_cast<int>(plot_parent->getAttribute("zlog"));

  if (element_context->hasAttribute("x"))
    {
      auto x = static_cast<std::string>(element_context->getAttribute("x"));
      x_vec = GRM::get<std::vector<double>>((*context)[x]);
      cols = x_vec.size();
    }
  if (element_context->hasAttribute("y"))
    {
      auto y = static_cast<std::string>(element_context->getAttribute("y"));
      y_vec = GRM::get<std::vector<double>>((*context)[y]);
      rows = y_vec.size();
    }

  if (!element_context->hasAttribute("z"))
    throw NotFoundError("Heatmap series is missing required attribute z-data.\n");
  auto z = static_cast<std::string>(element_context->getAttribute("z"));
  z_vec = GRM::get<std::vector<double>>((*context)[z]);
  z_length = z_vec.size();

  if (x_vec.empty() && y_vec.empty())
    {
      /* If neither `x` nor `y` are given, we need more information about the shape of `z` */
      if (!element->hasAttribute("zdims_min") || !element->hasAttribute("zdims_max"))
        throw NotFoundError("Heatmap series is missing required attribute zdims.\n");
      rows = static_cast<int>(element->getAttribute("zdims_min"));
      cols = static_cast<int>(element->getAttribute("zdims_max"));
    }
  else if (x_vec.empty())
    {
      cols = z_length / rows;
    }
  else if (y_vec.empty())
    {
      rows = z_length / cols;
    }

  is_uniform_heatmap = (x_vec.empty() || is_equidistant_array(cols, &(x_vec[0]))) &&
                       (y_vec.empty() || is_equidistant_array(rows, &(y_vec[0])));
  if (!is_uniform_heatmap && (x_vec.empty() || y_vec.empty()))
    throw NotFoundError("Heatmap series is missing x- or y-data or the data has to be uniform.\n");

  if (x_vec.empty())
    {
      x_min = static_cast<double>(element->getAttribute("xrange_min"));
      x_max = static_cast<double>(element->getAttribute("xrange_max"));
    }
  else
    {
      x_min = x_vec[0];
      x_max = x_vec[cols - 1];
    }
  if (y_vec.empty())
    {
      y_min = static_cast<double>(element->getAttribute("yrange_min"));
      y_max = static_cast<double>(element->getAttribute("yrange_max"));
    }
  else
    {
      y_min = y_vec[0];
      y_max = y_vec[rows - 1];
    }
  if (element_context->hasAttribute("marginalheatmap_kind"))
    {
      z_min = static_cast<double>(element_context->getAttribute("zrange_min"));
      z_max = static_cast<double>(element_context->getAttribute("zrange_max"));
    }
  else
    {
      z_min = static_cast<double>(element->getAttribute("zrange_min"));
      z_max = static_cast<double>(element->getAttribute("zrange_max"));
    }
  if (!element->hasAttribute("crange_min") || !element->hasAttribute("crange_max"))
    {
      c_min = z_min;
      c_max = z_max;
    }
  else
    {
      c_min = static_cast<double>(element->getAttribute("crange_min"));
      c_max = static_cast<double>(element->getAttribute("crange_max"));
    }

  if (zlog)
    {
      z_min = log(z_min);
      z_max = log(z_max);
      c_min = log(c_min);
      c_max = log(c_max);
    }

  if (!is_uniform_heatmap)
    {
      --cols;
      --rows;
    }
  for (i = 0; i < 256; i++)
    {
      gr_inqcolor(1000 + i, icmap + i);
    }

  data = std::vector<int>(rows * cols);
  if (z_max > z_min)
    {
      for (i = 0; i < cols * rows; i++)
        {
          if (zlog)
            {
              zv = log(z_vec[i]);
            }
          else
            {
              zv = z_vec[i];
            }

          if (zv > z_max || zv < z_min || grm_isnan(zv))
            {
              data[i] = -1;
            }
          else
            {
              data[i] = (int)((zv - c_min) / (c_max - c_min) * 255 + 0.5);
              if (data[i] >= 255)
                {
                  data[i] = 255;
                }
              else if (data[i] < 0)
                {
                  data[i] = 0;
                }
            }
        }
    }
  else
    {
      for (i = 0; i < cols * rows; i++)
        {
          data[i] = 0;
        }
    }
  rgba = std::vector<int>(rows * cols);

  /* clear old heatmaps */
  del = del_values(static_cast<int>(element->getAttribute("_delete_children")));
  clearOldChildren(&del, element);

  int id = (int)global_root->getAttribute("_id");
  global_root->setAttribute("_id", id + 1);
  std::string str = std::to_string(id);
  if (is_uniform_heatmap)
    {
      for (i = 0; i < rows * cols; i++)
        {
          if (data[i] == -1)
            {
              rgba[i] = 0;
            }
          else
            {
              rgba[i] = (255 << 24) + icmap[data[i]];
            }
        }

      std::vector<double> x_vec_tmp, y_vec_tmp;
      linspace(x_min, x_max, cols, x_vec_tmp);
      linspace(y_min, y_max, rows, y_vec_tmp);

      (*context)["x" + str] = x_vec_tmp;
      element->setAttribute("x", "x" + str);
      (*context)["y" + str] = y_vec_tmp;
      element->setAttribute("y", "y" + str);

      std::shared_ptr<GRM::Element> cellarray;
      if (del != del_values::update_without_default && del != del_values::update_with_default)
        {
          cellarray = global_render->createDrawImage(x_min, y_min, x_max, y_max, cols, rows, "rgba" + str, rgba, 0);
          cellarray->setAttribute("_child_id", child_id++);
          element->append(cellarray);
        }
      else
        {
          cellarray = element->querySelectors("[_child_id=" + std::to_string(child_id++) + "]");
          if (cellarray != nullptr)
            global_render->createDrawImage(x_min, y_min, x_max, y_max, cols, rows, "rgba" + str, rgba, 0, nullptr,
                                           cellarray);
        }
    }
  else
    {
      for (i = 0; i < rows * cols; i++)
        {
          if (data[i] == -1)
            {
              rgba[i] = 1256 + 1; /* Invalid color index -> gr_nonuniformcellarray draws a transparent rectangle */
            }
          else
            {
              rgba[i] = data[i] + 1000;
            }
        }

      std::shared_ptr<GRM::Element> cellarray;
      if (del != del_values::update_without_default && del != del_values::update_with_default)
        {
          cellarray = global_render->createNonUniformCellArray("x" + str, x_vec, "y" + str, y_vec, cols, rows, 1, 1,
                                                               cols, rows, "color" + str, rgba);
          cellarray->setAttribute("_child_id", child_id++);
          element->append(cellarray);
        }
      else
        {
          cellarray = element->querySelectors("[_child_id=" + std::to_string(child_id++) + "]");
          if (cellarray != nullptr)
            global_render->createNonUniformCellArray("x" + str, x_vec, "y" + str, y_vec, cols, rows, 1, 1, cols, rows,
                                                     "color" + str, rgba, nullptr, cellarray);
        }
    }
}

static void hexbin(const std::shared_ptr<GRM::Element> &element, const std::shared_ptr<GRM::Context> &context)
{
  /*!
   * Processing function for hexbin
   *
   * \param[in] element The GRM::Element that contains the attributes and data keys
   * \param[in] context The GRM::Context that contains the actual data
   */
  auto x = static_cast<std::string>(element->getAttribute("x"));
  auto y = static_cast<std::string>(element->getAttribute("y"));
  int nbins = static_cast<int>(element->getAttribute("nbins"));

  double *x_p = &(GRM::get<std::vector<double>>((*context)[x])[0]);
  double *y_p = &(GRM::get<std::vector<double>>((*context)[y])[0]);

  std::vector<double> x_vec = GRM::get<std::vector<double>>((*context)[x]);
  std::vector<double> y_vec = GRM::get<std::vector<double>>((*context)[y]);
  int x_length = x_vec.size();

  if (element->hasAttribute("_hexbin_context_address"))
    {
      auto address = static_cast<std::string>(element->getAttribute("_hexbin_context_address"));
      long hex_address = stol(address, nullptr, 16);
      const hexbin_2pass_t *hexbinContext = (hexbin_2pass_t *)hex_address;
      bool cleanup = hexbinContext->action & GR_2PASS_CLEANUP;
      if (redrawws) gr_hexbin_2pass(x_length, x_p, y_p, nbins, hexbinContext);
      if (cleanup)
        {
          element->removeAttribute("_hexbin_context_address");
        }
    }
  else
    {
      if (redrawws) gr_hexbin(x_length, x_p, y_p, nbins);
    }
}

static void processHexbin(const std::shared_ptr<GRM::Element> &element, const std::shared_ptr<GRM::Context> &context)
{
  int nbins = PLOT_DEFAULT_HEXBIN_NBINS;

  if (!element->hasAttribute("x")) throw NotFoundError("Hexbin series is missing required attribute x-data.\n");
  auto x = static_cast<std::string>(element->getAttribute("x"));
  if (!element->hasAttribute("y")) throw NotFoundError("Hexbin series is missing required attribute y-data.\n");
  auto y = static_cast<std::string>(element->getAttribute("y"));
  if (element->hasAttribute("nbins"))
    {
      nbins = static_cast<int>(element->getAttribute("nbins"));
    }
  else
    {
      element->setAttribute("nbins", nbins);
    }

  double *x_p = &(GRM::get<std::vector<double>>((*context)[x])[0]);
  double *y_p = &(GRM::get<std::vector<double>>((*context)[y])[0]);

  std::vector<double> x_vec = GRM::get<std::vector<double>>((*context)[x]);
  std::vector<double> y_vec = GRM::get<std::vector<double>>((*context)[y]);
  int x_length = x_vec.size();
  int y_length = y_vec.size();
  if (x_length != y_length) throw std::length_error("For Hexbin x- and y-data must have the same size\n.");

  if (redrawws)
    {
      const hexbin_2pass_t *hexbinContext = gr_hexbin_2pass(x_length, x_p, y_p, nbins, nullptr);

      std::ostringstream get_address;
      get_address << hexbinContext;
      element->setAttribute("_hexbin_context_address", get_address.str());

      auto colorbar = element->querySelectors("colorbar");
      double c_min = 0.0;
      double c_max = hexbinContext->cntmax;
      auto plot_parent = element->parentElement();

      getPlotParent(plot_parent);
      plot_parent->setAttribute("_clim_min", c_min);
      plot_parent->setAttribute("_clim_max", c_max);
      PushDrawableToZQueue pushHexbinToZQueue(hexbin);
      pushHexbinToZQueue(element, context);
    }
}

static void histBins(const std::shared_ptr<GRM::Element> &element, const std::shared_ptr<GRM::Context> &context)
{
  int current_point_count;
  double *tmp_bins;
  std::vector<double> x, weights;
  unsigned int num_bins = 0, num_weights;
  if (!element->hasAttribute("x")) throw NotFoundError("Hist series is missing required attribute x-data.\n");
  auto key = static_cast<std::string>(element->getAttribute("x"));
  x = GRM::get<std::vector<double>>((*context)[key]);
  current_point_count = x.size();

  if (element->hasAttribute("nbins")) num_bins = static_cast<int>(element->getAttribute("nbins"));
  if (element->hasAttribute("weights"))
    {
      auto weights_key = static_cast<std::string>(element->getAttribute("weights"));
      weights = GRM::get<std::vector<double>>((*context)[weights_key]);
      num_weights = weights.size();
    }
  if (!weights.empty() && current_point_count != num_weights)
    throw std::length_error("For hist series the size of data and weights must be the same.\n");
  if (num_bins <= 1)
    {
      num_bins = (int)(3.3 * log10(current_point_count) + 0.5) + 1;
    }
  auto bins = std::vector<double>(num_bins);
  double *x_p = &(x[0]);
  double *weights_p = (weights.empty()) ? nullptr : &(weights[0]);
  tmp_bins = &(bins[0]);
  bin_data(current_point_count, x_p, num_bins, tmp_bins, weights_p);
  std::vector<double> tmp(tmp_bins, tmp_bins + num_bins);

  int id = static_cast<int>(global_root->getAttribute("_id"));
  std::string str = std::to_string(id);
  (*context)["bins" + str] = tmp;
  element->setAttribute("bins", "bins" + str);
  global_root->setAttribute("_id", ++id);
}

static void processHist(const std::shared_ptr<GRM::Element> &element, const std::shared_ptr<GRM::Context> &context)
{
  /*!
   * Processing function for hist
   *
   * \param[in] element The GRM::Element that contains the attributes and data keys
   * \param[in] context The GRM::Context that contains the actual data
   */

  int bar_color_index = 989, i;
  std::vector<double> bar_color_rgb_vec = {-1, -1, -1};
  std::shared_ptr<GRM::Element> plot_parent;
  del_values del = del_values::update_without_default;
  int child_id = 0;
  int edge_color_index = 1;
  std::vector<double> edge_color_rgb_vec = {-1, -1, -1};
  double x_min, x_max, bar_width, y_min, y_max;
  std::vector<double> bins_vec;
  unsigned int num_bins;
  std::string orientation = PLOT_DEFAULT_ORIENTATION;
  bool is_horizontal;

  auto bar_color_rgb = static_cast<std::string>(element->getAttribute("bar_color_rgb"));
  bar_color_rgb_vec = GRM::get<std::vector<double>>((*context)[bar_color_rgb]);

  bar_color_index = static_cast<int>(element->getAttribute("bar_color_index"));

  if (element->parentElement()->localName() != "plot")
    {
      plot_parent = element->parentElement()->parentElement();
    }
  else
    {
      plot_parent = element->parentElement();
    }

  if (bar_color_rgb_vec[0] != -1)
    {
      for (i = 0; i < 3; i++)
        {
          if (bar_color_rgb_vec[i] > 1 || bar_color_rgb_vec[i] < 0)
            throw std::out_of_range("For hist series bar_color_rgb must be inside [0, 1].\n");
        }
      bar_color_index = 1000;
      global_render->setColorRep(element, bar_color_index, bar_color_rgb_vec[0], bar_color_rgb_vec[1],
                                 bar_color_rgb_vec[2]);
      // processcolorrep has to be manually triggered.
      processColorReps(element);
    }

  auto edge_color_rgb = static_cast<std::string>(element->getAttribute("edge_color_rgb"));
  edge_color_rgb_vec = GRM::get<std::vector<double>>((*context)[edge_color_rgb]);

  edge_color_index = static_cast<int>(element->getAttribute("edge_color_index"));
  if (edge_color_rgb_vec[0] != -1)
    {
      for (i = 0; i < 3; i++)
        {
          if (edge_color_rgb_vec[i] > 1 || edge_color_rgb_vec[i] < 0)
            throw std::out_of_range("For hist series edge_color_rgb must be inside [0, 1].\n");
        }
      edge_color_index = 1001;
      global_render->setColorRep(element, edge_color_index, edge_color_rgb_vec[0], edge_color_rgb_vec[1],
                                 edge_color_rgb_vec[2]);
      processColorReps(element);
    }

  if (!element->hasAttribute("bins")) histBins(element, context);
  auto bins = static_cast<std::string>(element->getAttribute("bins"));
  bins_vec = GRM::get<std::vector<double>>((*context)[bins]);
  num_bins = bins_vec.size();

  if (element->hasAttribute("orientation"))
    {
      orientation = static_cast<std::string>(element->getAttribute("orientation"));
    }
  else
    {
      element->setAttribute("orientation", orientation);
    }
  is_horizontal = orientation == "horizontal";

  x_min = static_cast<double>(element->getAttribute("xrange_min"));
  x_max = static_cast<double>(element->getAttribute("xrange_max"));
  y_min = static_cast<double>(element->getAttribute("yrange_min"));
  y_max = static_cast<double>(element->getAttribute("yrange_max"));
  if (std::isnan(y_min)) y_min = 0.0;

  if (element->parentElement()->hasAttribute("marginalheatmap_kind"))
    {
      if (element->parentElement()->hasAttribute("xrange_min"))
        x_min = static_cast<double>(element->parentElement()->getAttribute("xrange_min"));
      if (element->parentElement()->hasAttribute("xrange_max"))
        x_max = static_cast<double>(element->parentElement()->getAttribute("xrange_max"));
      if (element->parentElement()->hasAttribute("yrange_min"))
        y_min = static_cast<double>(element->parentElement()->getAttribute("yrange_min"));
      if (element->parentElement()->hasAttribute("yrange_max"))
        y_max = static_cast<double>(element->parentElement()->getAttribute("yrange_max"));
      element->setAttribute("calc_window_and_viewport_from_parent", 1);
      processCalcWindowAndViewportFromParent(element);
      processMarginalheatmapKind(element->parentElement());

      if (orientation == "vertical")
        {
          double tmp_min = x_min, tmp_max = x_max;

          x_min = y_min;
          x_max = y_max;
          y_min = tmp_min;
          y_max = tmp_max;
        }
      y_min = 0.0;
    }

  bar_width = (x_max - x_min) / num_bins;

  /* clear old bars */
  del = del_values(static_cast<int>(element->getAttribute("_delete_children")));
  clearOldChildren(&del, element);

  for (i = 1; i < num_bins + 1; ++i)
    {
      double x = x_min + (i - 1) * bar_width;
      std::shared_ptr<GRM::Element> bar;

      if (is_horizontal)
        {
          if (del != del_values::update_without_default && del != del_values::update_with_default)
            {
              bar =
                  global_render->createBar(x, x + bar_width, y_min, bins_vec[i - 1], bar_color_index, edge_color_index);
              bar->setAttribute("_child_id", child_id++);
              element->append(bar);
            }
          else
            {
              bar = element->querySelectors("[_child_id=" + std::to_string(child_id++) + "]");
              if (bar != nullptr)
                global_render->createBar(x, x + bar_width, y_min, bins_vec[i - 1], bar_color_index, edge_color_index,
                                         "", "", -1, "", bar);
            }
        }
      else
        {
          if (del != del_values::update_without_default && del != del_values::update_with_default)
            {
              bar =
                  global_render->createBar(y_min, bins_vec[i - 1], x, x + bar_width, bar_color_index, edge_color_index);
              bar->setAttribute("_child_id", child_id++);
              element->append(bar);
            }
          else
            {
              bar = element->querySelectors("[_child_id=" + std::to_string(child_id++) + "]");
              if (bar != nullptr)
                global_render->createBar(y_min, bins_vec[i - 1], x, x + bar_width, bar_color_index, edge_color_index,
                                         "", "", -1, "", bar);
            }
        }
    }

  // errorbar handling
  for (const auto &child : element->children())
    {
      if (child->localName() == "errorbars")
        {
          std::vector<double> bar_centers(num_bins);
          linspace(x_min + 0.5 * bar_width, x_max - 0.5 * bar_width, num_bins, bar_centers);
          extendErrorbars(child, context, bar_centers, bins_vec);
        }
    }
}

static void processBar(const std::shared_ptr<GRM::Element> &element, const std::shared_ptr<GRM::Context> &context)
{
  bool is_horizontal;
  double x1, x2, y1, y2;
  int bar_color_index, edge_color_index, text_color_index = -1, color_save_spot = PLOT_CUSTOM_COLOR_INDEX;
  std::shared_ptr<GRM::Element> fillRect, drawRect, text_elem;
  std::string orientation = PLOT_DEFAULT_ORIENTATION, text;
  del_values del = del_values::update_without_default;
  double linewidth = NAN, y_lightness = NAN;
  std::vector<double> bar_color_rgb, edge_color_rgb;
  int child_id = 0;

  x1 = static_cast<double>(element->getAttribute("x1"));
  x2 = static_cast<double>(element->getAttribute("x2"));
  y1 = static_cast<double>(element->getAttribute("y1"));
  y2 = static_cast<double>(element->getAttribute("y2"));
  bar_color_index = static_cast<int>(element->getAttribute("bar_color_index"));
  edge_color_index = static_cast<int>(element->getAttribute("edge_color_index"));
  if (element->hasAttribute("text")) text = static_cast<std::string>(element->getAttribute("text"));
  if (element->hasAttribute("linewidth")) linewidth = static_cast<double>(element->getAttribute("linewidth"));

  if (element->hasAttribute("bar_color_rgb"))
    {
      auto bar_color_rgb_key = static_cast<std::string>(element->getAttribute("bar_color_rgb"));
      bar_color_rgb = GRM::get<std::vector<double>>((*context)[bar_color_rgb_key]);
    }
  if (element->hasAttribute("edge_color_rgb"))
    {
      auto edge_color_rgb_key = static_cast<std::string>(element->getAttribute("edge_color_rgb"));
      edge_color_rgb = GRM::get<std::vector<double>>((*context)[edge_color_rgb_key]);
    }

  /* clear old rects */
  del = del_values(static_cast<int>(element->getAttribute("_delete_children")));
  clearOldChildren(&del, element);

  if (del != del_values::update_without_default && del != del_values::update_with_default)
    {
      fillRect = global_render->createFillRect(x1, x2, y1, y2);
      fillRect->setAttribute("_child_id", child_id++);
      element->append(fillRect);
    }
  else
    {
      fillRect = element->querySelectors("[_child_id=" + std::to_string(child_id++) + "]");
      if (fillRect != nullptr) global_render->createFillRect(x1, x2, y1, y2, 0, 0, -1, fillRect);
    }
  if (fillRect != nullptr)
    {
      global_render->setFillIntStyle(fillRect, GKS_K_INTSTYLE_SOLID);

      if (!bar_color_rgb.empty() && bar_color_rgb[0] != -1)
        {
          global_render->setColorRep(fillRect, color_save_spot, bar_color_rgb[0], bar_color_rgb[1], bar_color_rgb[2]);
          bar_color_index = color_save_spot;
          processColorReps(fillRect);
        }
      global_render->setFillColorInd(fillRect, bar_color_index);

      if (!text.empty())
        {
          int color;
          if (bar_color_index != -1)
            {
              color = bar_color_index;
            }
          else if (element->hasAttribute("fillcolorind"))
            {
              color = (int)element->getAttribute("fillcolorind");
            }
          y_lightness = getLightness(color);
        }
    }

  if (del != del_values::update_without_default)
    {
      drawRect = global_render->createDrawRect(x1, x2, y1, y2);
      drawRect->setAttribute("_child_id", child_id++);
      element->append(drawRect);
    }
  else
    {
      drawRect = element->querySelectors("[_child_id=" + std::to_string(child_id++) + "]");
      if (drawRect != nullptr) global_render->createDrawRect(x1, x2, y1, y2, drawRect);
    }
  if (drawRect != nullptr)
    {
      drawRect->setAttribute("z_index", 2);

      if (!edge_color_rgb.empty() && edge_color_rgb[0] != -1)
        {
          global_render->setColorRep(drawRect, color_save_spot - 1, edge_color_rgb[0], edge_color_rgb[1],
                                     edge_color_rgb[2]);
          edge_color_index = color_save_spot - 1;
        }
      if (element->parentElement()->localName() == "series_barplot")
        element->parentElement()->setAttribute("edge_color", edge_color_index);
      global_render->setLineColorInd(drawRect, edge_color_index);
      processLineColorInd(drawRect);
      if (!std::isnan(linewidth)) global_render->setLineWidth(drawRect, linewidth);
    }

  if (!text.empty())
    {
      if (del != del_values::update_without_default && del != del_values::update_with_default)
        {
          text_elem = global_render->createText((x1 + x2) / 2, (y1 + y2) / 2, text, CoordinateSpace::WC);
          text_elem->setAttribute("_child_id", child_id++);
          element->append(text_elem);
        }
      else
        {
          text_elem = element->querySelectors("[_child_id=" + std::to_string(child_id++) + "]");
          if (text_elem != nullptr)
            global_render->createText((x1 + x2) / 2, (y1 + y2) / 2, text, CoordinateSpace::WC, text_elem);
        }
      if (text_elem != nullptr)
        {
          text_elem->setAttribute("z_index", 2);
          global_render->setTextAlign(text_elem, 2, 3);
          global_render->setTextWidthAndHeight(text_elem, x2 - x1, y2 - y1);
          if (!std::isnan(y_lightness)) global_render->setTextColorInd(text_elem, (y_lightness < 0.4) ? 0 : 1);
        }
    }
}

static void processIsosurfaceRender(const std::shared_ptr<GRM::Element> &element,
                                    const std::shared_ptr<GRM::Context> &context)
{
  double viewport[4];
  double x_min, x_max, y_min, y_max;
  int fig_width, fig_height;
  int subplot_width, subplot_height;
  int drawable_type;

  drawable_type = static_cast<int>(element->getAttribute("drawable_type"));

  gr_inqviewport(&viewport[0], &viewport[1], &viewport[2], &viewport[3]);

  x_min = viewport[0];
  x_max = viewport[1];
  y_min = viewport[2];
  y_max = viewport[3];

  GRM::Render::get_figure_size(&fig_width, &fig_height, nullptr, nullptr);
  subplot_width = (int)(grm_max(fig_width, fig_height) * (x_max - x_min));
  subplot_height = (int)(grm_max(fig_width, fig_height) * (y_max - y_min));

  logger((stderr, "viewport: (%lf, %lf, %lf, %lf)\n", x_min, x_max, y_min, y_max));
  logger((stderr, "viewport ratio: %lf\n", (x_min - x_max) / (y_min - y_max)));
  logger((stderr, "subplot size: (%d, %d)\n", subplot_width, subplot_height));
  logger((stderr, "subplot ratio: %lf\n", ((double)subplot_width / (double)subplot_height)));

  gr3_drawimage((float)x_min, (float)x_max, (float)y_min, (float)y_max, subplot_width, subplot_height,
                GR3_DRAWABLE_GKS);
}

static void processLayoutGrid(const std::shared_ptr<GRM::Element> &element,
                              const std::shared_ptr<GRM::Context> &context)
{
  double xmin, xmax, ymin, ymax;
  xmin = (double)element->getAttribute("plot_xmin");
  xmax = (double)element->getAttribute("plot_xmax");
  ymin = (double)element->getAttribute("plot_ymin");
  ymax = (double)element->getAttribute("plot_ymax");

  gr_setviewport(xmin, xmax, ymin, ymax);
}

static void processLayoutGridElement(const std::shared_ptr<GRM::Element> &element,
                                     const std::shared_ptr<GRM::Context> &context)
{
  // todo is layoutgridelement actually needed? Can it be replaced by an ordinary group
  auto xmin = static_cast<double>(element->getAttribute("plot_xmin"));
  auto xmax = static_cast<double>(element->getAttribute("plot_xmax"));
  auto ymin = static_cast<double>(element->getAttribute("plot_ymin"));
  auto ymax = static_cast<double>(element->getAttribute("plot_ymax"));
}

static void processNonUniformPolarCellArray(const std::shared_ptr<GRM::Element> &element,
                                            const std::shared_ptr<GRM::Context> &context)
{
  auto x_org = static_cast<double>(element->getAttribute("x_org"));
  auto y_org = static_cast<double>(element->getAttribute("y_org"));
  auto phi_key = static_cast<std::string>(element->getAttribute("phi"));
  auto r_key = static_cast<std::string>(element->getAttribute("r"));
  auto dimr = static_cast<int>(element->getAttribute("dimr"));
  auto dimphi = static_cast<int>(element->getAttribute("dimphi"));
  auto scol = static_cast<int>(element->getAttribute("scol"));
  auto srow = static_cast<int>(element->getAttribute("srow"));
  auto ncol = static_cast<int>(element->getAttribute("ncol"));
  auto nrow = static_cast<int>(element->getAttribute("nrow"));
  auto color_key = static_cast<std::string>(element->getAttribute("color"));

  auto r_vec = GRM::get<std::vector<double>>((*context)[r_key]);
  auto phi_vec = GRM::get<std::vector<double>>((*context)[phi_key]);
  auto color_vec = GRM::get<std::vector<int>>((*context)[color_key]);

  double *phi = &(phi_vec[0]);
  double *r = &(r_vec[0]);
  int *color = &(color_vec[0]);

  if (redrawws) gr_nonuniformpolarcellarray(x_org, y_org, phi, r, dimphi, dimr, scol, srow, ncol, nrow, color);
}

static void processNonuniformcellarray(const std::shared_ptr<GRM::Element> &element,
                                       const std::shared_ptr<GRM::Context> &context)
{
  /*!
   * Processing function for nonuniformcellarray
   *
   * \param[in] element The GRM::Element that contains the attributes and data keys
   * \param[in] context The GRM::Context that contains the actual data
   */
  auto x = static_cast<std::string>(element->getAttribute("x"));
  auto y = static_cast<std::string>(element->getAttribute("y"));

  auto dimx = static_cast<int>(element->getAttribute("dimx"));
  auto dimy = static_cast<int>(element->getAttribute("dimy"));
  auto scol = static_cast<int>(element->getAttribute("scol"));
  auto srow = static_cast<int>(element->getAttribute("srow"));
  auto ncol = static_cast<int>(element->getAttribute("ncol"));
  auto nrow = static_cast<int>(element->getAttribute("nrow"));
  auto color = static_cast<std::string>(element->getAttribute("color"));

  auto x_p = (double *)&(GRM::get<std::vector<double>>((*context)[x])[0]);
  auto y_p = (double *)&(GRM::get<std::vector<double>>((*context)[y])[0]);

  auto color_p = (int *)&(GRM::get<std::vector<int>>((*context)[color])[0]);
  if (redrawws) gr_nonuniformcellarray(x_p, y_p, dimx, dimy, scol, srow, ncol, nrow, color_p);
}

static void processPanzoom(const std::shared_ptr<GRM::Element> &element, const std::shared_ptr<GRM::Context> &context)
{
  ; /* panzoom is being processed in the processLimits routine */
}

static void processPolarCellArray(const std::shared_ptr<GRM::Element> &element,
                                  const std::shared_ptr<GRM::Context> &context)
{
  auto x_org = static_cast<double>(element->getAttribute("x_org"));
  auto y_org = static_cast<double>(element->getAttribute("y_org"));
  auto phimin = static_cast<double>(element->getAttribute("phimin"));
  auto phimax = static_cast<double>(element->getAttribute("phimax"));
  auto rmin = static_cast<double>(element->getAttribute("rmin"));
  auto rmax = static_cast<double>(element->getAttribute("rmax"));
  auto dimr = static_cast<int>(element->getAttribute("dimr"));
  auto dimphi = static_cast<int>(element->getAttribute("dimphi"));
  auto scol = static_cast<int>(element->getAttribute("scol"));
  auto srow = static_cast<int>(element->getAttribute("srow"));
  auto ncol = static_cast<int>(element->getAttribute("ncol"));
  auto nrow = static_cast<int>(element->getAttribute("nrow"));
  auto color_key = static_cast<std::string>(element->getAttribute("color"));

  auto color_vec = GRM::get<std::vector<int>>((*context)[color_key]);
  int *color = &(color_vec[0]);

  if (redrawws)
    gr_polarcellarray(x_org, y_org, phimin, phimax, rmin, rmax, dimphi, dimr, scol, srow, ncol, nrow, color);
}

static void processPolyline(const std::shared_ptr<GRM::Element> &element, const std::shared_ptr<GRM::Context> &context)
{
  /*!
   * Processing Function for polyline
   *
   * \param[in] element The GRM::Element that contains the attributes and data keys
   * \param[in] context The GRM::Context that contains the actual data
   */
  if (element->getAttribute("x").isString() && element->getAttribute("y").isString())
    {
      auto x = static_cast<std::string>(element->getAttribute("x"));
      auto y = static_cast<std::string>(element->getAttribute("y"));

      std::vector<double> x_vec = GRM::get<std::vector<double>>((*context)[x]);
      std::vector<double> y_vec = GRM::get<std::vector<double>>((*context)[y]);

      int n = std::min<int>(x_vec.size(), y_vec.size());
      auto group = element->parentElement();
      if ((element->hasAttribute("linetypes") || element->hasAttribute("linewidths") ||
           element->hasAttribute("linecolorinds")) ||
          ((parentTypes.count(group->localName())) &&
           (group->hasAttribute("linetypes") || group->hasAttribute("linewidths") ||
            group->hasAttribute("linecolorinds"))))
        {
          lineHelper(element, context, "polyline");
        }
      else if (redrawws)
        gr_polyline(n, (double *)&(x_vec[0]), (double *)&(y_vec[0]));
    }
  else if (element->getAttribute("x1").isDouble() && element->getAttribute("x2").isDouble() &&
           element->getAttribute("y1").isDouble() && element->getAttribute("y2").isDouble())
    {
      auto x1 = static_cast<double>(element->getAttribute("x1"));
      auto x2 = static_cast<double>(element->getAttribute("x2"));
      auto y1 = static_cast<double>(element->getAttribute("y1"));
      auto y2 = static_cast<double>(element->getAttribute("y2"));
      double x[2] = {x1, x2};
      double y[2] = {y1, y2};

      if (redrawws) gr_polyline(2, x, y);
    }
}

static void processPolyline3d(const std::shared_ptr<GRM::Element> &element,
                              const std::shared_ptr<GRM::Context> &context)
{
  /*!
   * Processing function for polyline3d
   *
   * \param[in] element The GRM::Element that contains the attributes and data keys
   * \param[in] context The GRM::Context that contains the actual data
   */
  auto x = static_cast<std::string>(element->getAttribute("x"));
  auto y = static_cast<std::string>(element->getAttribute("y"));
  auto z = static_cast<std::string>(element->getAttribute("z"));

  std::vector<double> x_vec = GRM::get<std::vector<double>>((*context)[x]);
  std::vector<double> y_vec = GRM::get<std::vector<double>>((*context)[y]);
  std::vector<double> z_vec = GRM::get<std::vector<double>>((*context)[z]);

  double *x_p = &(x_vec[0]);
  double *y_p = &(y_vec[0]);
  double *z_p = &(z_vec[0]);
  auto group = element->parentElement();

  if ((element->hasAttribute("linetypes") || element->hasAttribute("linewidths") ||
       element->hasAttribute("linecolorinds")) ||
      ((parentTypes.count(group->localName())) &&
       (group->hasAttribute("linetypes") || group->hasAttribute("linewidths") || group->hasAttribute("linecolorinds"))))
    {
      lineHelper(element, context, "polyline3d");
    }
  else
    {
      if (redrawws) gr_polyline3d(x_vec.size(), x_p, y_p, z_p);
    }
}

static void processPolymarker(const std::shared_ptr<GRM::Element> &element,
                              const std::shared_ptr<GRM::Context> &context)
{
  /*!
   * Processing function for polymarker
   *
   * \param[in] element The GRM::Element that contains the attributes and data keys
   * \param[in] context The GRM::Context that contains the actual data
   */
  if (element->getAttribute("x").isString() && element->getAttribute("y").isString())
    {
      auto x = static_cast<std::string>(element->getAttribute("x"));
      auto y = static_cast<std::string>(element->getAttribute("y"));

      std::vector<double> x_vec = GRM::get<std::vector<double>>((*context)[x]);
      std::vector<double> y_vec = GRM::get<std::vector<double>>((*context)[y]);

      int n = std::min<int>(x_vec.size(), y_vec.size());
      auto group = element->parentElement();
      if ((element->hasAttribute("markertypes") || element->hasAttribute("markersizes") ||
           element->hasAttribute("markercolorinds")) ||
          (parentTypes.count(group->localName()) &&
           (group->hasAttribute("markertypes") || group->hasAttribute("markersizes") ||
            group->hasAttribute("markercolorinds"))))
        {
          markerHelper(element, context, "polymarker");
        }
      else
        {
          if (redrawws) gr_polymarker(n, (double *)&(x_vec[0]), (double *)&(y_vec[0]));
        }
    }
  else if (element->getAttribute("x").isDouble() && element->getAttribute("y").isDouble())
    {
      double x = static_cast<double>(element->getAttribute("x"));
      double y = static_cast<double>(element->getAttribute("y"));
      if (redrawws) gr_polymarker(1, &x, &y);
    }
}

static void processPolymarker3d(const std::shared_ptr<GRM::Element> &element,
                                const std::shared_ptr<GRM::Context> &context)
{
  /*!
   * Processing function for polymarker3d
   *
   * \param[in] element The GRM::Element that contains the attributes and data keys
   * \param[in] context The GRM::Context that contains the actual data
   */
  auto x = static_cast<std::string>(element->getAttribute("x"));
  auto y = static_cast<std::string>(element->getAttribute("y"));
  auto z = static_cast<std::string>(element->getAttribute("z"));

  std::vector<double> x_vec = GRM::get<std::vector<double>>((*context)[x]);
  std::vector<double> y_vec = GRM::get<std::vector<double>>((*context)[y]);
  std::vector<double> z_vec = GRM::get<std::vector<double>>((*context)[z]);

  double *x_p = &(x_vec[0]);
  double *y_p = &(y_vec[0]);
  double *z_p = &(z_vec[0]);

  auto group = element->parentElement();
  if ((element->hasAttribute("markertypes") || element->hasAttribute("markersizes") ||
       element->hasAttribute("markercolorinds")) ||
      (parentTypes.count(group->localName()) &&
       (group->hasAttribute("markertypes") || group->hasAttribute("markersizes") ||
        group->hasAttribute("markercolorinds"))))
    {
      markerHelper(element, context, "polymarker3d");
    }
  else
    {
      if (redrawws) gr_polymarker3d(x_vec.size(), x_p, y_p, z_p);
    }
}

static void processQuiver(const std::shared_ptr<GRM::Element> &element, const std::shared_ptr<GRM::Context> &context)
{
  /*!
   * Processing function for quiver
   *
   * \param[in] element The GRM::Element that contains the attributes and data keys
   * \param[in] context The GRM::Context that contains the actual data
   */
  if (!element->hasAttribute("x")) throw NotFoundError("Quiver series is missing required attribute x-data.\n");
  auto x = static_cast<std::string>(element->getAttribute("x"));
  if (!element->hasAttribute("y")) throw NotFoundError("Quiver series is missing required attribute y-data.\n");
  auto y = static_cast<std::string>(element->getAttribute("y"));
  if (!element->hasAttribute("u")) throw NotFoundError("Quiver series is missing required attribute u-data.\n");
  auto u = static_cast<std::string>(element->getAttribute("u"));
  if (!element->hasAttribute("v")) throw NotFoundError("Quiver series is missing required attribute v-data.\n");
  auto v = static_cast<std::string>(element->getAttribute("v"));
  int color = static_cast<int>(element->getAttribute("color"));

  std::vector<double> x_vec = GRM::get<std::vector<double>>((*context)[x]);
  std::vector<double> y_vec = GRM::get<std::vector<double>>((*context)[y]);
  std::vector<double> u_vec = GRM::get<std::vector<double>>((*context)[u]);
  std::vector<double> v_vec = GRM::get<std::vector<double>>((*context)[v]);
  int x_length = x_vec.size();
  int y_length = y_vec.size();
  int u_length = u_vec.size();
  int v_length = v_vec.size();

  if (x_length * y_length != u_length)
    throw std::length_error("For quiver series x_length * y_length must be u_length.\n");
  if (x_length * y_length != v_length)
    throw std::length_error("For quiver series x_length * y_length must be v_length.\n");

  double *x_p = &(x_vec[0]);
  double *y_p = &(y_vec[0]);
  double *u_p = &(GRM::get<std::vector<double>>((*context)[u])[0]);
  double *v_p = &(GRM::get<std::vector<double>>((*context)[v])[0]);

  if (redrawws) gr_quiver(x_length, y_length, x_p, y_p, u_p, v_p, color);
}

static void processPolar(const std::shared_ptr<GRM::Element> &element, const std::shared_ptr<GRM::Context> &context)
{
  /*!
   * Processing function for polar
   *
   * \param[in] element The GRM::Element that contains the attributes and data keys
   * \param[in] context The GRM::Context that contains the actual data
   */
  double r_min, r_max, tick;
  int n;
  unsigned int rho_length, theta_length;
  std::string spec = SERIES_DEFAULT_SPEC;
  unsigned int i;
  std::vector<double> theta_vec, rho_vec;
  auto plot_parent = element->parentElement();
  del_values del = del_values::update_without_default;
  int child_id = 0;

  getPlotParent(plot_parent);
  r_min = static_cast<double>(plot_parent->getAttribute("_ylim_min"));
  r_max = static_cast<double>(plot_parent->getAttribute("_ylim_max"));

  if (element->hasAttribute("spec"))
    {
      spec = static_cast<std::string>(element->getAttribute("spec"));
    }
  else
    {
      element->setAttribute("spec", spec);
    }
  element->setAttribute("r_min", r_min);
  element->setAttribute("r_max", r_max);
  tick = 0.5 * auto_tick(r_min, r_max);
  n = (int)ceil((r_max - r_min) / tick);
  r_max = r_min + n * tick;

  if (!element->hasAttribute("x")) throw NotFoundError("Polar series is missing required attribute x-data (theta).\n");
  auto x_key = static_cast<std::string>(element->getAttribute("x"));
  if (!element->hasAttribute("y")) throw NotFoundError("Polar series is missing required attribute y-data (rho).\n");
  auto y_key = static_cast<std::string>(element->getAttribute("y"));
  theta_vec = GRM::get<std::vector<double>>((*context)[x_key]);
  rho_vec = GRM::get<std::vector<double>>((*context)[y_key]);
  theta_length = theta_vec.size();
  rho_length = rho_vec.size();
  if (rho_length != theta_length)
    throw std::length_error("For polar series y(rho)- and x(theta)-data must have the same size.\n");

  std::vector<double> x(rho_length);
  std::vector<double> y(rho_length);

  for (i = 0; i < rho_length; ++i)
    {
      double current_rho = rho_vec[i] / r_max;
      x[i] = current_rho * cos(theta_vec[i]);
      y[i] = current_rho * sin(theta_vec[i]);
    }

  global_render->setLineSpec(element, spec);

  int id = (int)global_root->getAttribute("_id");
  global_root->setAttribute("_id", id + 1);

  /* clear old polylines */
  del = del_values(static_cast<int>(element->getAttribute("_delete_children")));
  clearOldChildren(&del, element);

  std::shared_ptr<GRM::Element> line;
  if (del != del_values::update_without_default && del != del_values::update_with_default)
    {
      line = global_render->createPolyline("x" + std::to_string(id), x, "y" + std::to_string(id), y);
      line->setAttribute("_child_id", child_id++);
      element->append(line);
    }
  else
    {
      line = element->querySelectors("[_child_id=" + std::to_string(child_id++) + "]");
      if (line != nullptr)
        global_render->createPolyline("x" + std::to_string(id), x, "y" + std::to_string(id), y, nullptr, 0, 0.0, 0,
                                      line);
    }
}

static void processPolarHeatmap(const std::shared_ptr<GRM::Element> &element,
                                const std::shared_ptr<GRM::Context> &context)
{
  /*!
   * Processing function for polar_heatmap
   *
   * \param[in] element The GRM::Element that contains the attributes and data keys
   * \param[in] context The GRM::Context that contains the actual data
   */

  std::string kind;
  int icmap[256], zlog = 0;
  unsigned int i, cols, rows, z_length;
  double x_min, x_max, y_min, y_max, z_min, z_max, c_min, c_max, zv;
  int is_uniform_heatmap;
  std::vector<int> data;
  std::vector<double> x_vec, y_vec, z_vec;
  del_values del = del_values::update_without_default;
  int child_id = 0;

  kind = static_cast<std::string>(element->parentElement()->getAttribute("kind"));
  zlog = static_cast<int>(element->parentElement()->getAttribute("zlog"));

  if (element->hasAttribute("x"))
    {
      auto x = static_cast<std::string>(element->getAttribute("x"));
      x_vec = GRM::get<std::vector<double>>((*context)[x]);
      cols = x_vec.size();
    }
  if (element->hasAttribute("y"))
    {
      auto y = static_cast<std::string>(element->getAttribute("y"));
      y_vec = GRM::get<std::vector<double>>((*context)[y]);
      rows = y_vec.size();
    }

  if (!element->hasAttribute("z")) throw NotFoundError("Polar-heatmap series is missing required attribute z-data.\n");
  auto z = static_cast<std::string>(element->getAttribute("z"));
  z_vec = GRM::get<std::vector<double>>((*context)[z]);
  z_length = z_vec.size();

  if (x_vec.empty() && y_vec.empty())
    {
      /* If neither `x` nor `y` are given, we need more information about the shape of `z` */
      if (!element->hasAttribute("zdims_min") || !element->hasAttribute("zdims_max"))
        throw NotFoundError("Polar-heatmap series is missing required attribute zdims.\n");
      rows = static_cast<int>(element->getAttribute("zdims_min"));
      cols = static_cast<int>(element->getAttribute("zdims_max"));
    }
  else if (x_vec.empty())
    {
      cols = z_length / rows;
    }
  else if (y_vec.empty())
    {
      rows = z_length / cols;
    }

  is_uniform_heatmap = is_equidistant_array(cols, &(x_vec[0])) && is_equidistant_array(rows, &(y_vec[0]));
  if (kind == "nonuniformpolar_heatmap") is_uniform_heatmap = 0;

  if (!is_uniform_heatmap && (x_vec.empty() || y_vec.empty()))
    throw NotFoundError("Polar-heatmap series is missing x- or y-data or the data has to be uniform.\n");

  if (x_vec.empty())
    {
      x_min = static_cast<double>(element->getAttribute("xrange_min"));
      x_max = static_cast<double>(element->getAttribute("xrange_max"));
    }
  else
    {
      x_min = x_vec[0];
      x_max = x_vec[cols - 1];
    }
  if (y_vec.empty())
    {
      y_min = static_cast<double>(element->getAttribute("yrange_min"));
      y_max = static_cast<double>(element->getAttribute("yrange_max"));
    }
  else
    {
      y_min = y_vec[0];
      y_max = y_vec[rows - 1];
    }

  z_min = static_cast<double>(element->getAttribute("zrange_min"));
  z_max = static_cast<double>(element->getAttribute("zrange_max"));
  if (!element->hasAttribute("crange_min") || !element->hasAttribute("crange_max"))
    {
      c_min = z_min;
      c_max = z_max;
    }
  else
    {
      c_min = static_cast<double>(element->getAttribute("crange_min"));
      c_max = static_cast<double>(element->getAttribute("crange_max"));
    }

  if (zlog)
    {
      z_min = log(z_min);
      z_max = log(z_max);
      c_min = log(c_min);
      c_max = log(c_max);
    }

  for (i = 0; i < 256; i++)
    {
      gr_inqcolor(1000 + i, icmap + i);
    }

  data = std::vector<int>(rows * cols);
  if (z_max > z_min)
    {
      for (i = 0; i < cols * rows; i++)
        {
          if (zlog)
            {
              zv = log(z_vec[i]);
            }
          else
            {
              zv = z_vec[i];
            }

          if (zv > z_max || zv < z_min || grm_isnan(zv))
            {
              data[i] = -1;
            }
          else
            {
              data[i] = 1000 + (int)(255.0 * (zv - c_min) / (c_max - c_min) + 0.5);
              if (data[i] >= 1255)
                {
                  data[i] = 1255;
                }
              else if (data[i] < 1000)
                {
                  data[i] = 1000;
                }
            }
        }
    }
  else
    {
      for (i = 0; i < cols * rows; i++)
        {
          data[i] = 0;
        }
    }

  int id = (int)global_root->getAttribute("_id");
  global_root->setAttribute("_id", id + 1);
  std::string str = std::to_string(id);

  /* clear old polar_heatmaps */
  del = del_values(static_cast<int>(element->getAttribute("_delete_children")));
  clearOldChildren(&del, element);

  std::shared_ptr<GRM::Element> polar_cellaray;
  if (is_uniform_heatmap)
    {
      if (del != del_values::update_without_default && del != del_values::update_with_default)
        {
          polar_cellaray = global_render->createPolarCellArray(0, 0, 0, 360, 0, 1, cols, rows, 1, 1, cols, rows,
                                                               "color" + str, data);
          polar_cellaray->setAttribute("_child_id", child_id++);
          element->append(polar_cellaray);
        }
      else
        {
          polar_cellaray = element->querySelectors("[_child_id=" + std::to_string(child_id++) + "]");
          if (polar_cellaray != nullptr)
            global_render->createPolarCellArray(0, 0, 0, 360, 0, 1, cols, rows, 1, 1, cols, rows, "color" + str, data,
                                                nullptr, polar_cellaray);
        }
    }
  else
    {
      y_min = static_cast<double>(element->parentElement()->getAttribute("window_ymin"));
      y_max = static_cast<double>(element->parentElement()->getAttribute("window_ymax"));

      std::vector<double> rho, phi;
      for (i = 0; i < ((cols > rows) ? cols : rows); ++i)
        {
          if (i < cols)
            {
              phi.push_back(x_vec[i] * 180 / M_PI);
            }
          if (i < rows)
            {
              rho.push_back(y_min + y_vec[i] / (y_max - y_min));
            }
        }

      if (del != del_values::update_without_default && del != del_values::update_with_default)
        {
          polar_cellaray = global_render->createNonUniformPolarCellArray(
              0, 0, "phi" + str, phi, "rho" + str, rho, -cols, -rows, 1, 1, cols, rows, "color" + str, data);
          polar_cellaray->setAttribute("_child_id", child_id++);
          element->append(polar_cellaray);
        }
      else
        {
          polar_cellaray = element->querySelectors("[_child_id=" + std::to_string(child_id++) + "]");
          if (polar_cellaray != nullptr)
            global_render->createNonUniformPolarCellArray(0, 0, "phi" + str, phi, "rho" + str, rho, -cols, -rows, 1, 1,
                                                          cols, rows, "color" + str, data, nullptr, polar_cellaray);
        }
    }
}

static void preBarplot(const std::shared_ptr<GRM::Element> &element, const std::shared_ptr<GRM::Context> &context)
{
  int max_y_length = 0;
  for (const auto &series : element->querySelectorsAll("series_barplot"))
    {
      if (!series->hasAttribute("indices")) throw NotFoundError("Barplot series is missing indices\n");
      auto indices_key = static_cast<std::string>(series->getAttribute("indices"));
      std::vector<int> indices_vec = GRM::get<std::vector<int>>((*context)[indices_key]);
      int cur_y_length = indices_vec.size();
      max_y_length = grm_max(cur_y_length, max_y_length);
    }
  element->setAttribute("max_y_length", max_y_length);
}

static void prePolarHistogram(const std::shared_ptr<GRM::Element> &element,
                              const std::shared_ptr<GRM::Context> &context)
{
  unsigned int num_bins, length, num_bin_edges, dummy;
  std::vector<double> theta;
  std::string norm = "count";
  std::vector<int> classes, bin_counts;
  double interval, start, max, temp_max, bin_width;
  double *p, *philim = nullptr;
  int maxObservations = 0, totalObservations = 0;
  std::vector<double> bin_edges, bin_widths;
  bool is_bin_counts = false;
  double philim_arr[2];
  std::vector<double> new_theta, new_edges;

  // element is the plot element -> get the first series with polarhistogram
  auto seriesList = element->querySelectorsAll("series_polar_histogram");
  std::shared_ptr<GRM::Element> group = seriesList[0];
  // element == plot_group for better readability
  const std::shared_ptr<GRM::Element> &plot_group = element;

  //! define keys for later usages;
  auto str = static_cast<std::string>(group->getAttribute("_id"));
  std::string bin_widths_key = "bin_widths" + str, bin_edges_key = "bin_edges" + str, classes_key = "classes" + str;

  if (group->hasAttribute("bin_counts"))
    {
      is_bin_counts = true;
      auto bin_counts_key = static_cast<std::string>(group->getAttribute("bin_counts"));
      bin_counts = GRM::get<std::vector<int>>((*context)[bin_counts_key]);

      length = bin_counts.size();
      num_bins = length;
      group->setAttribute("nbins", static_cast<int>(num_bins));
    }
  else if (group->hasAttribute("theta"))
    {
      auto theta_key = static_cast<std::string>(group->getAttribute("theta"));
      theta = GRM::get<std::vector<double>>((*context)[theta_key]);
      length = theta.size();
    }

  if (plot_group->hasAttribute("philim_min") || plot_group->hasAttribute("philim_max"))
    {
      philim = philim_arr;
      philim[0] = static_cast<double>(plot_group->getAttribute("philim_min"));
      philim[1] = static_cast<double>(plot_group->getAttribute("philim_max"));

      if (philim[1] < philim[0])
        {
          std::swap(philim[0], philim[1]);
          plot_group->setAttribute("phiflip", 1);
        }
      if (philim[0] < 0.0 || philim[1] > 2 * M_PI) logger((stderr, "\"philim\" must be between 0 and 2 * pi\n"));
      plot_group->setAttribute("philim_min", philim[0]);
      plot_group->setAttribute("philim_max", philim[1]);
    }

  /* bin_edges and nbins */
  if (!group->hasAttribute("bin_edges"))
    {
      if (!group->hasAttribute("nbins"))
        {
          num_bins = grm_min(12, (int)(length * 1.0 / 2) - 1);
          group->setAttribute("nbins", static_cast<int>(num_bins));
        }
      else
        {
          num_bins = static_cast<int>(group->getAttribute("nbins"));
          if (num_bins <= 0 || num_bins > 200)
            {
              num_bins = grm_min(12, (int)(length * 1.0 / 2) - 1);
              group->setAttribute("nbins", static_cast<int>(num_bins));
            }
        }
      //! check philim again
      if (philim == nullptr)
        num_bin_edges = 0;
      else
        {
          //! if philim is given, it will create equidistant bin_edges from phi_min to phi_max
          bin_edges.resize(num_bins + 1);
          linspace(philim[0], philim[1], (int)num_bins + 1, bin_edges);
          num_bin_edges = num_bins + 1;
          (*context)[bin_edges_key] = bin_edges;
          group->setAttribute("bin_edges", bin_edges_key);
        }
    }
  else /* with bin_edges */
    {
      int temp = 0, i;

      bin_edges_key = static_cast<std::string>(group->getAttribute("bin_edges"));
      bin_edges = GRM::get<std::vector<double>>((*context)[bin_edges_key]);
      num_bin_edges = bin_edges.size();

      /* filter bin_edges */
      new_edges.resize(num_bin_edges);
      for (i = 0; i < num_bin_edges; ++i)
        {
          if (philim == nullptr) /* no philim */
            {
              if (0.0 <= bin_edges[i] && bin_edges[i] <= 2 * M_PI)
                {
                  new_edges[temp] = bin_edges[i];
                  temp++;
                }
              else
                {
                  logger((stderr, "Only values between 0 and 2 * pi allowed\n"));
                }
            }
          else
            {
              if (philim[0] <= bin_edges[i] && bin_edges[i] <= philim[1])
                {
                  new_edges[temp] = bin_edges[i];
                  temp++;
                }
            }
        }
      if (num_bin_edges > temp)
        {
          num_bin_edges = temp;
          bin_edges.resize(temp);
        }
      else
        {
          bin_edges = new_edges;
        }
      if (philim == nullptr) /* no philim */
        {
          num_bins = num_bin_edges - 1;
          group->setAttribute("nbins", static_cast<int>(num_bins));
        }
      else /* with philim and binedges */
        {
          if (num_bin_edges == 1)
            {
              logger((stderr, "Given \"philim\" and given \"bin_edges\" are not compatible --> filtered "
                              "\"len(bin_edges) == 1\"\n"));
            }
          else
            {
              num_bins = num_bin_edges - 1;
              group->setAttribute("nbins", static_cast<int>(num_bins));
              group->setAttribute("bin_edges", bin_edges_key);
              (*context)[bin_edges_key] = bin_edges;
            }
        }
    }

  if (group->hasAttribute("normalization"))
    {
      norm = static_cast<std::string>(group->getAttribute("normalization"));
      if (!str_equals_any(norm.c_str(), 6, "count", "countdensity", "pdf", "probability", "cumcount", "cdf"))
        {
          logger((stderr, "Got keyword \"norm\"  with invalid value \"%s\"\n", norm.c_str()));
        }
    }

  if (!group->hasAttribute("bin_width"))
    {
      if (num_bin_edges > 0)
        {
          bin_widths.resize(num_bins + 1);
          for (int i = 1; i <= num_bin_edges - 1; ++i)
            {
              bin_widths[i - 1] = bin_edges[i] - bin_edges[i - 1];
            }
          group->setAttribute("bin_widths", bin_widths_key);
          (*context)[bin_widths_key] = bin_widths;
        }
      else
        {
          bin_width = 2 * M_PI / num_bins;
          group->setAttribute("bin_width", bin_width);
        }
    }
  else /* bin_width is given*/
    {
      int n = 0, temp;

      bin_width = static_cast<double>(group->getAttribute("bin_width"));

      if (num_bin_edges > 0 && philim == nullptr)
        {
          int i;
          logger((stderr, "\"bin_width\" is not compatible with \"bin_edges\"\n"));

          bin_widths.resize(num_bins);

          for (i = 1; i <= num_bin_edges - 1; ++i)
            {
              bin_widths[i - 1] = bin_edges[i] - bin_edges[i - 1];
            }
          group->setAttribute("bin_widths", bin_widths_key);
          (*context)[bin_widths_key] = bin_widths;
        }

      if (bin_width <= 0 || bin_width > 2 * M_PI)
        logger((stderr, "\"bin_width\" must be between 0 (exclusive) and 2 * pi\n"));

      /* with philim (with bin_width) */
      if (philim != nullptr)
        {
          if (philim[1] - philim[0] < bin_width)
            {
              logger((stderr, "The given \"philim\" range does not work with the given \"bin_width\"\n"));
            }
          else
            {
              n = (int)((philim[1] - philim[0]) / bin_width);
              if (is_bin_counts)
                {
                  if (num_bins > n)
                    logger((stderr, "\"bin_width\" does not work with this specific \"bin_count\". \"nbins\" do not "
                                    "fit \"bin_width\"\n"));
                  n = (int)num_bins;
                }
              bin_edges.resize(n + 1);
              linspace(philim[0], n * bin_width, n + 1, bin_edges);
            }
        }
      else /* without philim */
        {
          if ((int)(2 * M_PI / bin_width) > 200)
            {
              n = 200;
              bin_width = 2 * M_PI / n;
            }
          n = (int)(2 * M_PI / bin_width);
          if (is_bin_counts)
            {
              if (num_bins > n)
                logger((stderr, "\"bin_width\" does not work with this specific \"bin_count\". \"nbins\" do not fit "
                                "\"bin_width\"\n"));
              n = (int)num_bins;
            }
          bin_edges.resize(n + 1);
          linspace(0.0, n * bin_width, n + 1, bin_edges);
        }
      group->setAttribute("nbins", n);
      num_bin_edges = n + 1;
      num_bins = n;
      group->setAttribute("bin_edges", bin_edges_key);
      (*context)[bin_edges_key] = bin_edges;
      group->setAttribute("bin_width", bin_width);
      bin_widths.resize(num_bins);

      for (temp = 0; temp < num_bins; ++temp)
        {
          bin_widths[temp] = bin_width;
        }
      group->setAttribute("bin_widths", bin_widths_key);
      (*context)[bin_widths_key] = bin_widths;
    }

  /* is_bin_counts */
  if (is_bin_counts)
    {
      double temp_max_bc = 0.0;
      int i, j, total = 0, prev = 0;

      if (num_bin_edges > 0 && num_bins != num_bin_edges - 1)
        {
          logger((stderr, "Number of bin_edges must be number of bin_counts + 1\n"));
        }

      total = std::accumulate(bin_counts.begin(), bin_counts.end(), 0);
      for (i = 0; i < num_bins; ++i)
        {
          // temp_max_bc is a potential maximum for all bins respecting the given norm
          if (num_bin_edges > 0) bin_width = bin_widths[i];

          if (norm == "pdf" && bin_counts[i] * 1.0 / (total * bin_width) > temp_max_bc)
            temp_max_bc = bin_counts[i] * 1.0 / (total * bin_width);
          else if (norm == "countdensity" && bin_counts[i] * 1.0 / (bin_width) > temp_max_bc)
            temp_max_bc = bin_counts[i] * 1.0 / (bin_width);
          else if (bin_counts[i] > temp_max_bc)
            temp_max_bc = bin_counts[i];
        }

      classes.resize(num_bins);

      // bin_counts is affected by cumulative norms --> bin_counts are summed in later bins
      if (str_equals_any(norm.c_str(), 2, "cdf", "cumcount"))
        {
          for (i = 0; i < num_bins; ++i)
            {
              classes[i] = bin_counts[i];
              if (i != 0) classes[i] += classes[i - 1];
            }
        }
      else
        {
          classes = bin_counts;
        }

      group->setAttribute("classes", classes_key);
      (*context)[classes_key] = classes;
      group->setAttribute("total", total);

      if (norm == "probability")
        max = temp_max_bc * 1.0 / total;
      else if (norm == "cdf")
        max = 1.0;
      else if (norm == "cumcount")
        max = total * 1.0;
      else
        max = temp_max_bc;
    }
  else /* no is_bin_counts */
    {
      int x;

      max = 0.0;
      classes.resize(num_bins);

      // prepare bin_edges
      if (num_bin_edges == 0) // no bin_edges --> create bin_edges for uniform code later
        {
          // linspace the bin_edges
          bin_edges.resize(num_bins + 1);
          linspace(0.0, 2 * M_PI, (int)num_bins + 1, bin_edges);
        }
      else // bin_edges given
        {
          // filter theta
          double edge_min = bin_edges[0], edge_max = bin_edges[num_bin_edges - 1];

          auto it = std::remove_if(theta.begin(), theta.end(), [edge_min, edge_max](double angle) {
            return (angle < edge_min || angle > edge_max);
          });
          theta.erase(it, theta.end());
          length = theta.size();
        }

      // calc classes
      for (x = 0; x < num_bins; ++x)
        {
          int observations = 0;

          // iterate theta --> filter angles for current bin
          for (int y = 0; y < length; ++y)
            {
              if (bin_edges[x] <= theta[y] && theta[y] < bin_edges[x + 1]) ++observations;
            }

          // differentiate between cumulative and non-cumulative norms
          classes[x] = observations;
          if (x != 0 && str_equals_any(norm.c_str(), 2, "cdf", "cumcount")) classes[x] += classes[x - 1];
          // update the total number of observations; used for some norms;
          totalObservations += observations;
        }

      // get maximum number of observation from all bins
      maxObservations = *std::max_element(classes.begin(), classes.end());

      group->setAttribute("classes", classes_key);
      (*context)[classes_key] = classes;
      group->setAttribute("total", totalObservations);

      // calculate the maximum from maxObservations respecting the norms;
      if (num_bin_edges == 0 && norm == "pdf") // no given bin_edges
        {
          max = maxObservations * 1.0 / (totalObservations * bin_width);
        }
      else if (num_bin_edges != 0 &&
               str_equals_any(norm.c_str(), 2, "pdf", "countdensity")) // calc maximum with given bin_edges
        {
          for (int i = 0; i < num_bins; ++i)
            {
              // temporary maximum respecting norm
              temp_max = classes[i];
              if (norm == "pdf")
                temp_max /= totalObservations * bin_widths[x];
              else if (norm == "countdensity")
                temp_max /= bin_widths[x];

              if (temp_max > max) max = temp_max;
            }
        }
      else if (str_equals_any(norm.c_str(), 2, "probability", "cdf"))
        {
          max = maxObservations * 1.0 / totalObservations;
        }
      else
        {
          max = maxObservations * 1.0;
        }
    } /* end classes and maximum */
  // set r_max (radius_max) in parent for later usages in polar_axes and polar_histogram
  group->parentElement()->setAttribute("r_max", max);
}

static void processPolarHistogram(const std::shared_ptr<GRM::Element> &element,
                                  const std::shared_ptr<GRM::Context> &context)
{
  unsigned int num_bins, num_bin_edges = 0;
  int edge_color = 1, face_color = 989;
  int totalObservations = 0;
  int xcolormap = -2, ycolormap = -2;
  int child_id = 0;
  double face_alpha = 0.75, bin_width = -1.0, max;
  double r_min = 0.0, r_max = 1.0;
  double *rlim = nullptr, *philim = nullptr;
  double philim_arr[2];
  bool draw_edges = false, stairs = false, phiflip = false;
  std::string norm = "count";
  std::vector<double> r_lim_vec;
  std::vector<double> bin_edges, bin_widths;
  std::vector<double> mlist, rectlist;
  std::vector<int> classes;
  std::shared_ptr<GRM::Element> plot_group = element->parentElement();
  del_values del = del_values::update_without_default;

  auto classes_key = static_cast<std::string>(element->getAttribute("classes"));
  classes = GRM::get<std::vector<int>>((*context)[classes_key]);

  /* clear old polar-histogram children */
  del = del_values(static_cast<int>(element->getAttribute("_delete_children")));
  clearOldChildren(&del, element);

  if (element->hasAttribute("edge_color")) edge_color = static_cast<int>(element->getAttribute("edge_color"));
  if (element->hasAttribute("face_color")) face_color = static_cast<int>(element->getAttribute("face_color"));
  if (element->hasAttribute("face_alpha")) face_alpha = static_cast<double>(element->getAttribute("face_alpha"));
  if (element->hasAttribute("normalization")) norm = static_cast<std::string>(element->getAttribute("normalization"));
  if (plot_group->hasAttribute("phiflip")) phiflip = static_cast<int>(plot_group->getAttribute("phiflip"));
  if (element->hasAttribute("draw_edges")) draw_edges = static_cast<int>(element->getAttribute("draw_edges"));
  num_bins = static_cast<int>(element->getAttribute("nbins"));
  max = static_cast<double>(element->parentElement()->getAttribute("r_max"));
  totalObservations = static_cast<int>(element->getAttribute("total"));
  global_render->setTransparency(element, face_alpha);
  processTransparency(element);

  if (plot_group->hasAttribute("philim_min") || plot_group->hasAttribute("philim_max"))
    {
      philim = philim_arr;
      philim[0] = static_cast<double>(plot_group->getAttribute("philim_min"));
      philim[1] = static_cast<double>(plot_group->getAttribute("philim_max"));
    }

  if (!element->hasAttribute("bin_edges"))
    {
      if (element->hasAttribute("bin_width")) bin_width = static_cast<double>(element->getAttribute("bin_width"));
    }
  else
    {
      auto bin_edges_key = static_cast<std::string>(element->getAttribute("bin_edges"));
      bin_edges = GRM::get<std::vector<double>>((*context)[bin_edges_key]);
      num_bin_edges = bin_edges.size();

      auto bin_widths_key = static_cast<std::string>(element->getAttribute("bin_widths"));
      bin_widths = GRM::get<std::vector<double>>((*context)[bin_widths_key]);
      num_bins = bin_widths.size();
    }

  if (element->hasAttribute("stairs"))
    {
      /* Set default stairs line color width and alpha values */
      if (!element->hasAttribute("face_alpha")) face_alpha = 1.0;

      stairs = static_cast<int>(element->getAttribute("stairs"));
      if (stairs)
        {
          if (draw_edges)
            {
              logger((stderr, "\"stairs\" is not compatible with \"draw_edges\" / colormap\n"));
            }
          else if (num_bin_edges == 0) /* no bin_edges */
            {
              mlist.resize(num_bins * 4);
            }
          else
            {
              rectlist.resize(num_bins);
            }
        }
    }

  if (plot_group->hasAttribute("rlim_min") && plot_group->hasAttribute("rlim_max"))
    {
      r_lim_vec.push_back(static_cast<double>(plot_group->getAttribute("rlim_min")));
      r_lim_vec.push_back(static_cast<double>(plot_group->getAttribute("rlim_max")));
      rlim = &(r_lim_vec[0]);

      mlist.resize((num_bins + 1) * 4);
      r_min = grm_min(rlim[0], rlim[1]);
      r_max = grm_max(rlim[0], rlim[1]);
      if (rlim[0] > rlim[1])
        {
          rlim[0] = r_min;
          rlim[1] = r_max;
        }

      if (r_max > 1.0)
        {
          r_max = 1.0;
          logger((stderr, "The value of \"rlim_max\" can not exceed 1.0\n"));
        }
      if (r_min < 0.0) r_min = 0.0;
    }

  if (phiflip) std::reverse(classes.begin(), classes.end());

  /* if phiflip and bin_edges are given --> invert the angles*/
  if (phiflip && num_bin_edges > 0)
    {
      int u;
      std::vector<double> temp(num_bin_edges), temp2(num_bins);

      for (u = 0; u < num_bin_edges; u++)
        {
          temp[u] = 2 * M_PI - bin_edges[num_bin_edges - 1 - u];
        }
      for (u = (int)num_bins - 1; u >= 0; --u)
        {
          temp2[u] = bin_widths[num_bins - 1 - u];
        }
      bin_widths = temp2;
      bin_edges = temp;
    }

  for (int class_nr = 0; class_nr < classes.size(); ++class_nr)
    {
      double count = classes[class_nr];
      if (classes[class_nr] == 0)
        {
          /* stairs bin_edges / philim  */
          if (!rectlist.empty() && philim != nullptr)
            rectlist[class_nr] = r_min;
          else if (!rectlist.empty())
            rectlist[class_nr] = 0.0;
        }

      if (str_equals_any(norm.c_str(), 2, "probability", "cdf"))
        {
          count /= totalObservations;
        }
      else if (norm == "pdf")
        {
          if (num_bin_edges == 0)
            {
              count /= totalObservations * bin_width;
            }
          else
            {
              count /= (totalObservations * bin_widths[class_nr]);
            }
        }
      else if (norm == "countdensity")
        {
          if (num_bin_edges == 0)
            {
              count /= bin_width;
            }
          else
            {
              count /= bin_widths[class_nr];
            }
        }

      if (!(element->hasAttribute("xcolormap") && element->hasAttribute("ycolormap")))
        {
          if (draw_edges) logger((stderr, "\"draw_edges\" can only be used with colormap\n"));
        }
      else
        {
          xcolormap = static_cast<int>(element->getAttribute("xcolormap"));
          ycolormap = static_cast<int>(element->getAttribute("ycolormap"));
        }

      if (!stairs)
        {
          std::shared_ptr<GRM::Element> polar_bar;

          if (del != del_values::update_without_default && del != del_values::update_with_default)
            {
              polar_bar = global_render->createPolarBar(count, class_nr);
              polar_bar->setAttribute("_child_id", child_id++);
              element->append(polar_bar);
            }
          else
            {
              polar_bar = element->querySelectors("[_child_id=" + std::to_string(child_id++) + "]");
              if (polar_bar != nullptr) global_render->createPolarBar(count, class_nr, polar_bar);
            }

          if (polar_bar != nullptr)
            {
              if (bin_width != -1) polar_bar->setAttribute("bin_width", bin_width);
              if (norm != "count") polar_bar->setAttribute("norm", norm);
              if (phiflip) polar_bar->setAttribute("phiflip", phiflip);
              if (draw_edges) polar_bar->setAttribute("draw_edges", draw_edges);
              if (edge_color != 1) polar_bar->setAttribute("edge_color", edge_color);
              if (face_color != 989) polar_bar->setAttribute("face_color", face_color);
              if (xcolormap != -2) polar_bar->setAttribute("xcolormap", xcolormap);
              if (ycolormap != -2) polar_bar->setAttribute("ycolormap", ycolormap);
              if (!bin_widths.empty()) polar_bar->setAttribute("bin_widths", bin_widths[class_nr]);
              if (!bin_edges.empty())
                {
                  int id = static_cast<int>(global_root->getAttribute("_id"));
                  std::string str = std::to_string(id);
                  global_root->setAttribute("_id", id + 1);

                  auto bin_edges_vec = std::vector<double>{bin_edges[class_nr], bin_edges[class_nr + 1]};
                  auto bin_edges_key = "bin_edges" + str;
                  (*context)[bin_edges_key] = bin_edges_vec;
                  polar_bar->setAttribute("bin_edges", bin_edges_key);
                }
            }
        }
      else if (!draw_edges && (xcolormap == -2 && ycolormap == -2)) /* stairs without draw_edges (not compatible) */
        {
          double r, rect;
          std::complex<double> complex1, complex2;
          const double convert = 180.0 / M_PI;
          double edge_width = 2.3; /* only for stairs */

          global_render->setFillColorInd(element, 1);
          global_render->setLineColorInd(element, edge_color);
          global_render->setLineWidth(element, edge_width);
          processLineColorInd(element);
          processFillColorInd(element);
          processLineWidth(element);

          r = pow((count / max), (num_bins * 2));
          complex1 = moivre(r, (2 * class_nr), (int)num_bins * 2);
          complex2 = moivre(r, (2 * class_nr + 2), ((int)num_bins * 2));
          rect = sqrt(pow(real(complex1), 2) + pow(imag(complex1), 2));

          /*  no bin_edges */
          if (num_bin_edges == 0)
            {
              double arc_pos;
              std::shared_ptr<GRM::Element> arc;

              mlist[class_nr * 4] = real(complex1);
              mlist[class_nr * 4 + 1] = imag(complex1);
              mlist[class_nr * 4 + 2] = real(complex2);
              mlist[class_nr * 4 + 3] = imag(complex2);

              if (rlim != nullptr)
                {
                  for (int i = 0; i < 2; ++i)
                    {
                      double temporary =
                          fabs(sqrt(pow(mlist[class_nr * 4 + 2 - i * 2], 2) + pow(mlist[class_nr * 4 + 3 - i * 2], 2)));
                      if (temporary > r_max)
                        {
                          double factor = fabs(r_max / temporary);
                          mlist[class_nr * 4 + 2 - i * 2] *= factor;
                          mlist[class_nr * 4 + 3 - i * 2] *= factor;
                        }
                    }

                  if (rect > r_min)
                    {
                      if (del != del_values::update_without_default && del != del_values::update_with_default)
                        {
                          arc = global_render->createDrawArc(
                              -grm_min(rect, r_max), grm_min(rect, r_max), -grm_min(rect, r_max), grm_min(rect, r_max),
                              class_nr * (360.0 / num_bins), (class_nr + 1) * 360.0 / num_bins);
                          arc->setAttribute("_child_id", child_id++);
                          element->append(arc);
                        }
                      else
                        {
                          arc = element->querySelectors("[_child_id=" + std::to_string(child_id++) + "]");
                          if (arc != nullptr)
                            global_render->createDrawArc(-grm_min(rect, r_max), grm_min(rect, r_max),
                                                         -grm_min(rect, r_max), grm_min(rect, r_max),
                                                         class_nr * (360.0 / num_bins),
                                                         (class_nr + 1) * 360.0 / num_bins, arc);
                        }

                      arc_pos = r_min;
                    }
                }
              else /* no rlim */
                {
                  arc_pos = rect;
                }
              if (del != del_values::update_without_default && del != del_values::update_with_default)
                {
                  arc =
                      global_render->createDrawArc(-arc_pos, arc_pos, -arc_pos, arc_pos, class_nr * (360.0 / num_bins),
                                                   (class_nr + 1) * (360.0 / num_bins));
                  arc->setAttribute("_child_id", child_id++);
                  element->append(arc);
                }
              else
                {
                  arc = element->querySelectors("[_child_id=" + std::to_string(child_id++) + "]");
                  if (arc != nullptr)
                    global_render->createDrawArc(-arc_pos, arc_pos, -arc_pos, arc_pos, class_nr * (360.0 / num_bins),
                                                 (class_nr + 1) * (360.0 / num_bins), arc);
                }
            }
          else /* with bin_edges */
            {
              /* rlim and bin_edges*/
              std::shared_ptr<GRM::Element> arc;
              double arc_pos;

              if (rlim != nullptr)
                {
                  if (rect < r_min)
                    rectlist[class_nr] = r_min;
                  else if (rect > r_max)
                    rectlist[class_nr] = r_max;
                  else
                    rectlist[class_nr] = rect;

                  if (rect > r_min)
                    {
                      if (del != del_values::update_without_default && del != del_values::update_with_default)
                        {
                          arc = global_render->createDrawArc(
                              -grm_min(rect, r_max), grm_min(rect, r_max), -grm_min(rect, r_max), grm_min(rect, r_max),
                              bin_edges[class_nr] * convert, bin_edges[class_nr + 1] * convert);
                          arc->setAttribute("_child_id", child_id++);
                          element->append(arc);
                        }
                      else
                        {
                          arc = element->querySelectors("[_child_id=" + std::to_string(child_id++) + "]");
                          if (arc != nullptr)
                            global_render->createDrawArc(-grm_min(rect, r_max), grm_min(rect, r_max),
                                                         -grm_min(rect, r_max), grm_min(rect, r_max),
                                                         bin_edges[class_nr] * convert,
                                                         bin_edges[class_nr + 1] * convert, arc);
                        }

                      arc_pos = r_min;
                    }
                }
              else /* no rlim */
                {
                  rectlist[class_nr] = rect;
                  if (class_nr == num_bin_edges - 1) break;
                  arc_pos = rect;
                }
              if (del != del_values::update_without_default && del != del_values::update_with_default)
                {
                  arc = global_render->createDrawArc(-arc_pos, arc_pos, -arc_pos, arc_pos,
                                                     bin_edges[class_nr] * convert, bin_edges[class_nr + 1] * convert);
                  arc->setAttribute("_child_id", child_id++);
                  element->append(arc);
                }
              else
                {
                  arc = element->querySelectors("[_child_id=" + std::to_string(child_id++) + "]");
                  if (arc != nullptr)
                    global_render->createDrawArc(-arc_pos, arc_pos, -arc_pos, arc_pos, bin_edges[class_nr] * convert,
                                                 bin_edges[class_nr + 1] * convert, arc);
                }
            }
        }
    } /* end of classes for loop */

  if (stairs && !draw_edges && (xcolormap == -2 && ycolormap == -2))
    {
      std::shared_ptr<GRM::Element> line;
      double line_x[2], line_y[2];

      /* stairs without binedges, rlim */
      if (!mlist.empty() && rlim == nullptr && rectlist.empty())
        {
          for (int s = 0; s < num_bins * 4; s += 2)
            {
              if (s > 2 && s % 4 == 0)
                {
                  line_x[0] = mlist[s];
                  line_x[1] = mlist[s - 2];
                  line_y[0] = mlist[s + 1];
                  line_y[1] = mlist[s - 1];

                  if (del != del_values::update_without_default && del != del_values::update_with_default)
                    {
                      line = global_render->createPolyline(line_x[0], line_x[1], line_y[0], line_y[1]);
                      line->setAttribute("_child_id", child_id++);
                      element->append(line);
                    }
                  else
                    {
                      line = element->querySelectors("[_child_id=" + std::to_string(child_id++) + "]");
                      if (line != nullptr)
                        global_render->createPolyline(line_x[0], line_x[1], line_y[0], line_y[1], 0, 0.0, 0, line);
                    }
                }
            }
          line_x[0] = mlist[0];
          line_x[1] = mlist[(num_bins - 1) * 4 + 2];
          line_y[0] = mlist[1];
          line_y[1] = mlist[(num_bins - 1) * 4 + 3];
        }
      else if (!mlist.empty() && rlim != nullptr && rectlist.empty()) /* stairs without bin_edges with rlim*/
        {
          double rect1, rect2;

          for (int x = 0; x < num_bins; ++x)
            {
              rect1 = sqrt(pow(mlist[x * 4], 2) + pow(mlist[x * 4 + 1], 2));
              rect2 = sqrt(pow(mlist[(x - 1) * 4 + 2], 2) + pow(mlist[(x - 1) * 4 + 3], 2));

              if (rect1 < r_min && rect2 < r_min) continue;
              if (rect1 < r_min)
                {
                  mlist[4 * x] = r_min * cos(2 * M_PI / num_bins * x);
                  mlist[4 * x + 1] = r_min * sin(2 * M_PI / num_bins * x);
                }
              else if (rect2 < r_min)
                {
                  mlist[(x - 1) * 4 + 2] = r_min * cos(2 * M_PI / num_bins * x);
                  mlist[(x - 1) * 4 + 3] = r_min * sin(2 * M_PI / num_bins * x);
                }
              line_x[0] = mlist[x * 4];
              line_x[1] = mlist[(x - 1) * 4 + 2];
              line_y[0] = mlist[x * 4 + 1];
              line_y[1] = mlist[(x - 1) * 4 + 3];

              if (del != del_values::update_without_default && del != del_values::update_with_default)
                {
                  line = global_render->createPolyline(line_x[0], line_x[1], line_y[0], line_y[1]);
                  line->setAttribute("_child_id", child_id++);
                  element->append(line);
                }
              else
                {
                  line = element->querySelectors("[_child_id=" + std::to_string(child_id++) + "]");
                  if (line != nullptr)
                    global_render->createPolyline(line_x[0], line_x[1], line_y[0], line_y[1], 0, 0.0, 0, line);
                }
            }
          line_x[0] = mlist[(num_bins - 1) * 4 + 2] = grm_max(mlist[(num_bins - 1) * 4 + 2], r_min * cos(0));
          line_y[0] = mlist[(num_bins - 1) * 4 + 3] = grm_max(mlist[(num_bins - 1) * 4 + 3], r_min * sin(0));
          line_x[1] = mlist[0] = grm_max(mlist[0], r_min * cos(0));
          line_y[1] = mlist[1] = grm_max(mlist[1], r_min * sin(0));
        }
      else if (!rectlist.empty() && rlim == nullptr) /* stairs with binedges without rlim */
        {
          double startx = 0.0, starty = 0.0;

          for (int x = 0; x < num_bin_edges - 1; ++x)
            {
              line_x[0] = startx;
              line_x[1] = rectlist[x] * cos(bin_edges[x]);
              line_y[0] = starty;
              line_y[1] = rectlist[x] * sin(bin_edges[x]);

              startx = rectlist[x] * cos(bin_edges[x + 1]);
              starty = rectlist[x] * sin(bin_edges[x + 1]);

              if (!(bin_edges[0] == 0.0 && bin_edges[num_bin_edges - 1] > 1.96 * M_PI) || x > 0)
                {
                  if (del != del_values::update_without_default && del != del_values::update_with_default)
                    {
                      line = global_render->createPolyline(line_x[0], line_x[1], line_y[0], line_y[1]);
                      line->setAttribute("_child_id", child_id++);
                      element->append(line);
                    }
                  else
                    {
                      line = element->querySelectors("[_child_id=" + std::to_string(child_id++) + "]");
                      if (line != nullptr)
                        global_render->createPolyline(line_x[0], line_x[1], line_y[0], line_y[1], 0, 0.0, 0, line);
                    }
                }
            }

          if (bin_edges[0] == 0.0 && bin_edges[num_bin_edges - 1] > 1.96 * M_PI)
            {
              line_x[0] = rectlist[0] * cos(bin_edges[0]);
              line_x[1] = startx;
              line_y[0] = rectlist[0] * sin(bin_edges[0]);
              line_y[1] = starty;
            }
          else
            {
              line_x[0] = rectlist[num_bin_edges - 2] * cos(bin_edges[num_bin_edges - 1]);
              line_x[1] = 0.0;
              line_y[0] = rectlist[num_bin_edges - 2] * sin(bin_edges[num_bin_edges - 1]);
              line_y[1] = 0.0;
            }
        }
      else if (!rectlist.empty() && rlim != nullptr) /* stairs with bin_edges and rlim */
        {
          double startx = grm_max(rectlist[0] * cos(bin_edges[0]), r_min * cos(bin_edges[0]));
          double starty = grm_max(rectlist[0] * sin(bin_edges[0]), r_min * sin(bin_edges[0]));

          for (int x = 0; x < num_bin_edges - 1; ++x)
            {
              line_x[0] = startx;
              line_x[1] = rectlist[x] * cos(bin_edges[x]);
              line_y[0] = starty;
              line_y[1] = rectlist[x] * sin(bin_edges[x]);

              startx = rectlist[x] * cos(bin_edges[x + 1]);
              starty = rectlist[x] * sin(bin_edges[x + 1]);

              if ((!phiflip &&
                   (!((bin_edges[0] > 0.0 && bin_edges[0] < 0.001) && bin_edges[num_bin_edges - 1] > 1.96 * M_PI) ||
                    x > 0)) ||
                  ((bin_edges[0] > 1.96 * M_PI &&
                    !(bin_edges[num_bin_edges - 1] > 0.0 && bin_edges[num_bin_edges - 1] < 0.001)) ||
                   x > 0))
                {
                  if (del != del_values::update_without_default && del != del_values::update_with_default)
                    {
                      line = global_render->createPolyline(line_x[0], line_x[1], line_y[0], line_y[1]);
                      line->setAttribute("_child_id", child_id++);
                      element->append(line);
                    }
                  else
                    {
                      line = element->querySelectors("[_child_id=" + std::to_string(child_id++) + "]");
                      if (line != nullptr)
                        global_render->createPolyline(line_x[0], line_x[1], line_y[0], line_y[1], 0, 0.0, 0, line);
                    }
                }
            }

          if (!(bin_edges[0] == 0.0 && bin_edges[num_bin_edges - 1] > 1.96 * M_PI))
            {
              line_x[0] = r_min * cos(bin_edges[0]);
              line_x[1] = rectlist[0] * cos(bin_edges[0]);
              line_y[0] = r_min * sin(bin_edges[0]);
              line_y[1] = rectlist[0] * sin(bin_edges[0]);

              if (del != del_values::update_without_default && del != del_values::update_with_default)
                {
                  line = global_render->createPolyline(line_x[0], line_x[1], line_y[0], line_y[1]);
                  line->setAttribute("_child_id", child_id++);
                  element->append(line);
                }
              else
                {
                  line = element->querySelectors("[_child_id=" + std::to_string(child_id++) + "]");
                  if (line != nullptr)
                    global_render->createPolyline(line_x[0], line_x[1], line_y[0], line_y[1], 0, 0.0, 0, line);
                }
            }

          if (bin_edges[0] == 0.0 && bin_edges[num_bin_edges - 1] > 1.96 * M_PI)
            {
              line_x[0] = rectlist[0] * cos(bin_edges[0]);
              line_x[1] = rectlist[num_bin_edges - 2] * cos(bin_edges[num_bin_edges - 1]);
              line_y[0] = rectlist[0] * sin(bin_edges[0]);
              line_y[1] = rectlist[num_bin_edges - 2] * sin(bin_edges[num_bin_edges - 1]);
            }
          else
            {
              line_x[0] = rectlist[num_bin_edges - 2] * cos(bin_edges[num_bin_edges - 1]);
              line_x[1] = r_min * cos(bin_edges[num_bin_edges - 1]);
              line_y[0] = rectlist[num_bin_edges - 2] * sin(bin_edges[num_bin_edges - 1]);
              line_y[1] = r_min * sin(bin_edges[num_bin_edges - 1]);
            }
        }

      if (del != del_values::update_without_default && del != del_values::update_with_default)
        {
          line = global_render->createPolyline(line_x[0], line_x[1], line_y[0], line_y[1]);
          line->setAttribute("_child_id", child_id++);
          element->append(line);
        }
      else
        {
          line = element->querySelectors("[_child_id=" + std::to_string(child_id++) + "]");
          if (line != nullptr)
            global_render->createPolyline(line_x[0], line_x[1], line_y[0], line_y[1], 0, 0.0, 0, line);
        }
    }
}

static void processPolarBar(const std::shared_ptr<GRM::Element> &element, const std::shared_ptr<GRM::Context> &context)
{
  unsigned int resample;
  double *rlim = nullptr;
  double r, rect;
  std::complex<double> complex1, complex2;
  std::vector<double> angles, bin_edges;
  std::vector<int> colormap;
  const double convert = 180.0 / M_PI;
  std::vector<int> lineardata, bin_counts;
  std::vector<double> f1, f2, arc_2_x, arc_2_y;
  std::vector<double> phi_vec, r_lim_vec;
  std::complex<double> r_min_complex1, r_min_complex2;
  int child_id = 0;
  int xcolormap = -2, ycolormap = -2;
  double count, bin_width = -1.0, bin_widths;
  double r_min = 0.0, r_max = 1.0, max;
  int num_bins, num_bin_edges = 0, class_nr;
  std::string norm = "count", str;
  bool phiflip = false, draw_edges = false;
  int edge_color = 1, face_color = 989;
  std::vector<double> mlist = {0.0, 0.0, 0.0, 0.0};
  std::shared_ptr<GRM::Element> plot_elem = element->parentElement()->parentElement();
  del_values del = del_values::update_without_default;

  /* clear old polar-histogram children */
  del = del_values(static_cast<int>(element->getAttribute("_delete_children")));
  clearOldChildren(&del, element);

  class_nr = static_cast<int>(element->getAttribute("class_nr"));
  count = static_cast<double>(element->getAttribute("count"));

  if (element->hasAttribute("bin_width")) bin_width = static_cast<double>(element->getAttribute("bin_width"));
  if (element->hasAttribute("norm")) norm = static_cast<std::string>(element->getAttribute("norm"));
  if (element->hasAttribute("phiflip")) phiflip = static_cast<int>(element->getAttribute("phiflip"));
  if (element->hasAttribute("draw_edges")) draw_edges = static_cast<int>(element->getAttribute("draw_edges"));
  if (element->hasAttribute("edge_color")) edge_color = static_cast<int>(element->getAttribute("edge_color"));
  if (element->hasAttribute("face_color")) face_color = static_cast<int>(element->getAttribute("face_color"));
  if (element->hasAttribute("xcolormap")) xcolormap = static_cast<int>(element->getAttribute("xcolormap"));
  if (element->hasAttribute("ycolormap")) ycolormap = static_cast<int>(element->getAttribute("ycolormap"));
  if (element->hasAttribute("bin_edges"))
    {
      auto bin_edges_key = static_cast<std::string>(element->getAttribute("bin_edges"));
      bin_edges = GRM::get<std::vector<double>>((*context)[bin_edges_key]);
      num_bin_edges = bin_edges.size();
    }
  if (element->hasAttribute("bin_widths")) bin_widths = static_cast<double>(element->getAttribute("bin_widths"));

  num_bins = static_cast<int>(element->parentElement()->getAttribute("nbins"));
  if (element->parentElement()->hasAttribute("bin_widths"))
    {
      auto bin_widths_key = static_cast<std::string>(element->parentElement()->getAttribute("bin_widths"));
      auto bin_widths_vec = GRM::get<std::vector<double>>((*context)[bin_widths_key]);
      num_bins = bin_widths_vec.size();
    }

  if (plot_elem->hasAttribute("rlim_min") && plot_elem->hasAttribute("rlim_max"))
    {
      r_lim_vec.push_back(static_cast<double>(plot_elem->getAttribute("rlim_min")));
      r_lim_vec.push_back(static_cast<double>(plot_elem->getAttribute("rlim_max")));
      rlim = &(r_lim_vec[0]);

      r_min = grm_min(rlim[0], rlim[1]);
      r_max = grm_max(rlim[0], rlim[1]);
      if (rlim[0] > rlim[1])
        {
          rlim[0] = r_min;
          rlim[1] = r_max;
        }

      if (r_max > 1.0)
        {
          r_max = 1.0;
          logger((stderr, "The value of \"rlim_max\" can not exceed 1.0\n"));
        }
      if (r_min < 0.0) r_min = 0.0;
    }
  max = static_cast<double>(plot_elem->getAttribute("r_max"));

  if (element->hasAttribute("xcolormap") && element->hasAttribute("ycolormap"))
    {
      if (-1 > xcolormap || xcolormap > 47 || ycolormap < -1 || ycolormap > 47)
        {
          logger((stderr, "The value for keyword \"colormap\" must contain two integer between -1 and 47\n"));
        }
      else
        {
          std::shared_ptr<GRM::Element> drawImage;
          const int colormap_size = 500, image_size = 2000;
          double radius, angle, max_radius;
          int total = 0;
          double norm_factor = 1;
          int id = (int)global_root->getAttribute("_id");

          global_root->setAttribute("_id", id + 1);
          str = std::to_string(id);

          lineardata.resize(image_size * image_size);
          bin_counts.resize(num_bins);

          create_colormap(xcolormap, ycolormap, colormap_size, colormap);

          if (num_bin_edges == 0)
            {
              angles.resize(num_bins + 1);
              linspace(0.0, M_PI * 2, (int)num_bins + 1, angles);
            }
          else
            {
              angles = bin_edges;
            }
          max_radius = image_size / 2;

          total = static_cast<int>(element->getAttribute("total"));

          if (str_equals_any(norm.c_str(), 2, "probability", "cdf"))
            norm_factor = total;
          else if (num_bin_edges == 0 && norm == "pdf")
            norm_factor = total * bin_width;
          else if (num_bin_edges == 0 && norm == "countdensity")
            norm_factor = bin_width;

          if (rlim != nullptr)
            {
              r_min *= max_radius;
              r_max *= max_radius;
            }
          else
            {
              r_min = 0.0;
              r_max = max_radius;
            }

          for (int y = 0; y < image_size; y++)
            {
              for (int x = 0; x < image_size; x++)
                {
                  radius = sqrt(pow(x - max_radius, 2) + pow(y - max_radius, 2));
                  angle = atan2(y - max_radius, x - max_radius);

                  if (angle < 0) angle += M_PI * 2;
                  if (!phiflip) angle = 2 * M_PI - angle;

                  if (angle > angles[class_nr] && angle <= angles[class_nr + 1])
                    {
                      if (norm == "pdf" && num_bin_edges > 0)
                        norm_factor = total * bin_widths;
                      else if (norm == "countdensity" && num_bin_edges > 0)
                        norm_factor = bin_widths;

                      if ((grm_round(radius * 100) / 100) <=
                              (grm_round((count * 1.0 / norm_factor / max * max_radius) * 100) / 100) &&
                          radius <= r_max && radius > r_min)
                        {
                          lineardata[y * image_size + x] = colormap
                              [(int)(radius / (max_radius * pow(2, 0.5)) * (colormap_size - 1)) * colormap_size +
                               grm_max(grm_min((int)(angle / (2 * M_PI) * colormap_size), colormap_size - 1), 0)];
                        }

                    } /* end angle check */
                }     /* end x loop*/
            }         /* end y loop */
          if (rlim != nullptr)
            {
              r_min = rlim[0];
              r_max = rlim[1];
            }

          /* save resample method and reset because it isn't restored with gr_restorestate */
          if (del != del_values::update_without_default && del != del_values::update_with_default)
            {
              drawImage = global_render->createDrawImage(-1.0, 1.0, 1.0, -1.0, image_size, image_size, "data" + str,
                                                         lineardata, 0);
              drawImage->setAttribute("_child_id", child_id++);
              element->append(drawImage);
            }
          else
            {
              drawImage = element->querySelectors("[_child_id=" + std::to_string(child_id++) + "]");
              if (drawImage != nullptr)
                global_render->createDrawImage(-1.0, 1.0, 1.0, -1.0, image_size, image_size, "data" + str, lineardata,
                                               0, nullptr, drawImage);
            }
          gr_inqresamplemethod(&resample);
          if (drawImage != nullptr) global_render->setResampleMethod(drawImage, static_cast<int>(0x2020202));
          lineardata.clear();
          colormap.clear();
        }
    }

  /* perform calculations for later usages */
  r = pow((count / max), num_bins * 2);
  complex1 = moivre(r, 2 * class_nr, (int)num_bins * 2);

  rect = sqrt(pow(real(complex1), 2) + pow(imag(complex1), 2));

  if (rlim != nullptr)
    {
      complex2 = moivre(r, 2 * class_nr + 2, (int)num_bins * 2);

      mlist[0] = real(complex1);
      mlist[1] = imag(complex1);
      mlist[2] = real(complex2);
      mlist[3] = imag(complex2);

      r_min_complex1 = moivre(pow((r_min), (num_bins * 2)), (class_nr * 2), (int)num_bins * 2);
      r_min_complex2 = moivre(pow((r_min), (num_bins * 2)), (class_nr * 2 + 2), (int)num_bins * 2);

      /* check if the segment is higher than rmax? */
      for (int i = 0; i < 2; ++i)
        {
          double temporary = fabs(sqrt(pow(mlist[2 - i * 2], 2) + pow(mlist[3 - i * 2], 2)));
          if (temporary > r_max)
            {
              double factor = fabs(r_max / temporary);
              mlist[2 - i * 2] *= factor;
              mlist[3 - i * 2] *= factor;
            }
        }
      r = count / max;
      if (r > r_max) r = r_max;
    }

  /*  no binedges */
  if (num_bin_edges == 0)
    {
      if (rlim != nullptr)
        {
          int i, num_angle;
          double start_angle, end_angle;
          std::shared_ptr<GRM::Element> area;
          int id = (int)global_root->getAttribute("_id");

          if (r > r_min)
            {
              global_root->setAttribute("_id", id + 1);
              str = std::to_string(id);

              start_angle = class_nr * (360.0 / num_bins) / convert;
              end_angle = (class_nr + 1) * (360.0 / num_bins) / convert;

              // determine number of angles for arc approximations
              num_angle = (int)((end_angle - start_angle) / (0.2 / convert));

              phi_vec.resize(num_angle);
              linspace(start_angle, end_angle, num_angle, phi_vec);

              // 4 because of the 4 corner coordinates and 2 * num_angle for the arc approximations, top and
              // bottom
              f1.resize(4 + 2 * num_angle);
              /* line_1_x[0] and [1]*/
              f1[0] = real(r_min_complex1);
              f1[1] = mlist[0];
              /* arc_1_x */
              listcomprehension(r, cos, phi_vec, num_angle, 2, f1);
              /* reversed line_2_x [0] and [1] */
              f1[2 + num_angle + 1] = real(r_min_complex2);
              f1[2 + num_angle] = mlist[2];
              /* reversed arc_2_x */
              listcomprehension(r_min, cos, phi_vec, num_angle, 0, arc_2_x);
              for (i = 0; i < num_angle; ++i)
                {
                  f1[2 + num_angle + 2 + i] = arc_2_x[num_angle - 1 - i];
                }
              arc_2_x.clear();

              f2.resize(4 + 2 * num_angle);
              /* line_1_y[0] and [1] */
              f2[0] = imag(r_min_complex1);
              f2[1] = mlist[1];
              /*arc_1_y */
              listcomprehension(r, sin, phi_vec, num_angle, 2, f2);
              /* reversed line_2_y [0] and [1] */
              f2[2 + num_angle + 1] = imag(r_min_complex2);
              f2[2 + num_angle] = mlist[3];
              /* reversed arc_2_y */
              listcomprehension(r_min, sin, phi_vec, num_angle, 0, arc_2_y);
              for (i = 0; i < num_angle; ++i)
                {
                  f2[2 + num_angle + 2 + i] = arc_2_y[num_angle - 1 - i];
                }
              arc_2_y.clear();

              if (!draw_edges)
                {
                  // with rlim gr_fillarc cant be used because it will always draw from the origin
                  // instead use gr_fillarea and approximate line segment with calculations from above.
                  if (del != del_values::update_without_default && del != del_values::update_with_default)
                    {
                      area = global_render->createFillArea("x" + str, f1, "y" + str, f2);
                      area->setAttribute("_child_id", child_id++);
                      element->append(area);
                    }
                  else
                    {
                      area = element->querySelectors("[_child_id=" + std::to_string(child_id++) + "]");
                      if (area != nullptr)
                        global_render->createFillArea("x" + str, f1, "y" + str, f2, nullptr, 0, 0, -1, area);
                    }

                  if (area != nullptr)
                    {
                      global_render->setFillColorInd(area, face_color);
                      global_render->setFillIntStyle(area, 1);
                    }
                }

              // drawarea more likely
              if (del != del_values::update_without_default && del != del_values::update_with_default)
                {
                  area = global_render->createFillArea("x" + str, f1, "y" + str, f2);
                  area->setAttribute("_child_id", child_id++);
                  element->append(area);
                }
              else
                {
                  area = element->querySelectors("[_child_id=" + std::to_string(child_id++) + "]");
                  if (area != nullptr)
                    global_render->createFillArea("x" + str, f1, "y" + str, f2, nullptr, 0, 0, -1, area);
                }
              if (area != nullptr)
                {
                  global_render->setFillColorInd(area, edge_color);
                  global_render->setFillIntStyle(area, 0);
                  area->setAttribute("z_index", 2);
                }

              /* clean up vectors for next iteration */
              phi_vec.clear();
              f1.clear();
              f2.clear();
            }
        }  /* end rlim condition */
      else /* no rlim */
        {
          std::shared_ptr<GRM::Element> arc, drawArc;

          if (!draw_edges)
            {
              if (del != del_values::update_without_default && del != del_values::update_with_default)
                {
                  arc = global_render->createFillArc(-rect, rect, -rect, rect, class_nr * (360.0 / num_bins),
                                                     (class_nr + 1) * (360.0 / num_bins));
                  arc->setAttribute("_child_id", child_id++);
                  element->append(arc);
                }
              else
                {
                  arc = element->querySelectors("[_child_id=" + std::to_string(child_id++) + "]");
                  if (arc != nullptr)
                    global_render->createFillArc(-rect, rect, -rect, rect, class_nr * (360.0 / num_bins),
                                                 (class_nr + 1) * (360.0 / num_bins), 0, 0, -1, arc);
                }

              if (arc != nullptr)
                {
                  global_render->setFillIntStyle(arc, 1);
                  global_render->setFillColorInd(arc, face_color);
                }
            }

          if (del != del_values::update_without_default && del != del_values::update_with_default)
            {
              drawArc = global_render->createFillArc(-rect, rect, -rect, rect, class_nr * (360.0 / num_bins),
                                                     (class_nr + 1) * (360.0 / num_bins));
              drawArc->setAttribute("_child_id", child_id++);
              element->append(drawArc);
            }
          else
            {
              drawArc = element->querySelectors("[_child_id=" + std::to_string(child_id++) + "]");
              if (drawArc != nullptr)
                global_render->createFillArc(-rect, rect, -rect, rect, class_nr * (360.0 / num_bins),
                                             (class_nr + 1) * (360.0 / num_bins), 0, 0, -1, drawArc);
            }
          if (drawArc != nullptr)
            {
              global_render->setFillIntStyle(drawArc, 0);
              global_render->setFillColorInd(drawArc, edge_color);
              drawArc->setAttribute("z_index", 2);
            }
        }
    }
  else /* bin_egdes */
    {
      if (rlim != nullptr)
        {
          double start_angle, end_angle;
          int num_angle, i;
          std::shared_ptr<GRM::Element> area;
          int id = (int)global_root->getAttribute("_id");

          if (r > r_min)
            {
              global_root->setAttribute("_id", id + 1);
              str = std::to_string(id);

              start_angle = bin_edges[0];
              end_angle = bin_edges[1];

              num_angle = (int)((end_angle - start_angle) / (0.2 / convert));
              phi_vec.resize(num_angle);
              linspace(start_angle, end_angle, num_angle, phi_vec);

              f1.resize(4 + 2 * num_angle);
              /* line_1_x[0] and [1]*/
              f1[0] = cos(bin_edges[0]) * r_min;
              f1[1] = grm_min(rect, r_max) * cos(bin_edges[0]);
              /*arc_1_x */
              listcomprehension(r, cos, phi_vec, num_angle, 2, f1);
              /* reversed line_2_x [0] and [1] */
              f1[2 + num_angle + 1] = cos(bin_edges[1]) * r_min;
              f1[2 + num_angle] = grm_min(rect, r_max) * cos(bin_edges[1]);
              /* reversed arc_2_x */
              listcomprehension(r_min, cos, phi_vec, num_angle, 0, arc_2_x);
              for (i = 0; i < num_angle; ++i)
                {
                  f1[2 + num_angle + 2 + i] = arc_2_x[num_angle - 1 - i];
                }
              arc_2_x.clear();

              f2.resize(4 + 2 * num_angle);
              /* line_1_y[0] and [1] */
              f2[0] = r_min * sin(bin_edges[0]);
              f2[1] = grm_min(rect, r_max) * sin(bin_edges[0]);
              /*arc_1_y */
              listcomprehension(r, sin, phi_vec, num_angle, 2, f2);
              /* reversed line_2_y [0] and [1] */
              f2[2 + num_angle + 1] = r_min * sin(bin_edges[1]);
              f2[2 + num_angle] = grm_min(rect, r_max) * sin(bin_edges[1]);
              /* reversed arc_2_y */
              listcomprehension(r_min, sin, phi_vec, num_angle, 0, arc_2_y);
              for (i = 0; i < num_angle; ++i)
                {
                  f2[2 + num_angle + 2 + i] = arc_2_y[num_angle - 1 - i];
                }
              arc_2_y.clear();

              if (!draw_edges)
                {
                  if (del != del_values::update_without_default && del != del_values::update_with_default)
                    {
                      area = global_render->createFillArea("x" + str, f1, "y" + str, f2);
                      area->setAttribute("_child_id", child_id++);
                      element->append(area);
                    }
                  else
                    {
                      area = element->querySelectors("[_child_id=" + std::to_string(child_id++) + "]");
                      if (area != nullptr)
                        global_render->createFillArea("x" + str, f1, "y" + str, f2, nullptr, 0, 0, -1, area);
                    }

                  if (area != nullptr)
                    {
                      global_render->setFillColorInd(area, face_color);
                      global_render->setFillIntStyle(area, 1);
                    }
                }

              // drawarea more likely
              if (del != del_values::update_without_default && del != del_values::update_with_default)
                {
                  area = global_render->createFillArea("x" + str, f1, "y" + str, f2);
                  area->setAttribute("_child_id", child_id++);
                  element->append(area);
                }
              else
                {
                  area = element->querySelectors("[_child_id=" + std::to_string(child_id++) + "]");
                  if (area != nullptr)
                    global_render->createFillArea("x" + str, f1, "y" + str, f2, nullptr, 0, 0, -1, area);
                }
              if (area != nullptr)
                {
                  global_render->setFillColorInd(area, edge_color);
                  global_render->setFillIntStyle(area, 0);
                  area->setAttribute("z_index", 2);
                }

              /* clean up vectors for next iteration */
              phi_vec.clear();
              f1.clear();
              f2.clear();
            }
        }
      else /* no rlim */
        {
          std::shared_ptr<GRM::Element> arc, drawArc;

          if (!draw_edges)
            {
              if (del != del_values::update_without_default && del != del_values::update_with_default)
                {
                  arc = global_render->createFillArc(-rect, rect, -rect, rect, bin_edges[0] * convert,
                                                     bin_edges[1] * convert);
                  arc->setAttribute("_child_id", child_id++);
                  element->append(arc);
                }
              else
                {
                  arc = element->querySelectors("[_child_id=" + std::to_string(child_id++) + "]");
                  if (arc != nullptr)
                    global_render->createFillArc(-rect, rect, -rect, rect, bin_edges[0] * convert,
                                                 bin_edges[1] * convert, 0, 0, -1, arc);
                }

              if (arc != nullptr)
                {
                  global_render->setFillIntStyle(arc, 1);
                  global_render->setFillColorInd(arc, face_color);
                }
            }

          // drawarc
          if (del != del_values::update_without_default && del != del_values::update_with_default)
            {
              drawArc = global_render->createFillArc(-rect, rect, -rect, rect, bin_edges[0] * convert,
                                                     bin_edges[1] * convert);
              drawArc->setAttribute("_child_id", child_id++);
              element->append(drawArc);
            }
          else
            {
              drawArc = element->querySelectors("[_child_id=" + std::to_string(child_id++) + "]");
              if (drawArc != nullptr)
                global_render->createFillArc(-rect, rect, -rect, rect, bin_edges[0] * convert, bin_edges[1] * convert,
                                             0, 0, -1, drawArc);
            }
          if (drawArc != nullptr)
            {
              global_render->setFillIntStyle(drawArc, 0);
              global_render->setFillColorInd(drawArc, edge_color);
              drawArc->setAttribute("z_index", 2);
            }
        }
    }
}

static void processScatter(const std::shared_ptr<GRM::Element> &element, const std::shared_ptr<GRM::Context> &context)
{
  /*!
   * Processing function for scatter
   *
   * \param[in] element The GRM::Element that contains the attributes and data keys
   * \param[in] context The GRM::Context that contains the actual data
   */
  std::string orientation = PLOT_DEFAULT_ORIENTATION;
  double c_min, c_max;
  unsigned int x_length, y_length, z_length, c_length;
  int i, c_index = -1, markertype = -1;
  std::vector<int> markerColorIndsVec;
  std::vector<double> markerSizesVec;
  std::vector<double> x_vec, y_vec, z_vec, c_vec;
  del_values del = del_values::update_without_default;
  int child_id = 0;
  std::shared_ptr<GRM::Element> marker;

  if (!element->hasAttribute("x")) throw NotFoundError("Scatter series is missing required attribute x-data.\n");
  auto x = static_cast<std::string>(element->getAttribute("x"));
  if (!element->hasAttribute("y")) throw NotFoundError("Scatter series is missing required attribute y-data.\n");
  auto y = static_cast<std::string>(element->getAttribute("y"));
  x_vec = GRM::get<std::vector<double>>((*context)[x]);
  y_vec = GRM::get<std::vector<double>>((*context)[y]);
  x_length = x_vec.size();
  y_length = y_vec.size();
  if (x_length != y_length) throw std::length_error("For scatter series x- and y-data must have the same size.\n");

  if (element->hasAttribute("z"))
    {
      auto z = static_cast<std::string>(element->getAttribute("z"));
      z_vec = GRM::get<std::vector<double>>((*context)[z]);
      z_length = z_vec.size();
      if (x_length != z_length) throw std::length_error("For scatter series x- and z-data must have the same size.\n");
    }
  if (element->hasAttribute("c"))
    {
      auto c = static_cast<std::string>(element->getAttribute("c"));
      c_vec = GRM::get<std::vector<double>>((*context)[c]);
      c_length = c_vec.size();
    }
  if (element->hasAttribute("orientation"))
    {
      orientation = static_cast<std::string>(element->getAttribute("orientation"));
    }
  else
    {
      element->setAttribute("orientation", orientation);
    }
  auto is_horizontal = orientation == "horizontal";

  if (!element->hasAttribute("markertype"))
    {
      element->setAttribute("markertype", *previous_scatter_marker_type++);
      if (*previous_scatter_marker_type == INT_MAX)
        {
          previous_scatter_marker_type = plot_scatter_markertypes;
        }
    }
  processMarkerType(element);

  if (c_vec.empty() && element->hasAttribute("c_index"))
    {
      c_index = static_cast<int>(element->getAttribute("c_index"));
      if (c_index < 0)
        {
          logger((stderr, "Invalid scatter color %d, using 0 instead\n", c_index));
          c_index = 0;
        }
      else if (c_index > 255)
        {
          logger((stderr, "Invalid scatter color %d, using 255 instead\n", c_index));
          c_index = 255;
        }
    }

  /* clear old marker */
  del = del_values(static_cast<int>(element->getAttribute("_delete_children")));
  clearOldChildren(&del, element);

  if (!z_vec.empty() || !c_vec.empty())
    {
      auto plot_parent = element->parentElement();

      getPlotParent(plot_parent);
      c_min = static_cast<double>(plot_parent->getAttribute("_clim_min"));
      c_max = static_cast<double>(plot_parent->getAttribute("_clim_max"));

      for (i = 0; i < x_length; i++)
        {
          if (!z_vec.empty())
            {
              if (i < z_length)
                {
                  markerSizesVec.push_back(z_vec[i]);
                }
              else
                {
                  markerSizesVec.push_back(2.0);
                }
            }
          if (!c_vec.empty())
            {
              if (i < c_length)
                {
                  c_index = 1000 + (int)(255.0 * (c_vec[i] - c_min) / (c_max - c_min) + 0.5);
                  if (c_index < 1000 || c_index > 1255)
                    {
                      // colorind -1000 will be skipped
                      markerColorIndsVec.push_back(-1000);
                      continue;
                    }
                }
              else
                {
                  c_index = 989;
                }
              markerColorIndsVec.push_back(c_index);
            }
          else if (c_index != -1)
            {
              markerColorIndsVec.push_back(1000 + c_index);
            }
        }

      int id = static_cast<int>(global_root->getAttribute("_id"));
      std::string str = std::to_string(id);
      global_root->setAttribute("_id", ++id);

      if (is_horizontal)
        {
          if (del != del_values::update_without_default && del != del_values::update_with_default)
            {
              marker = global_render->createPolymarker("x" + str, x_vec, "y" + str, y_vec);
              marker->setAttribute("_child_id", child_id++);
              element->append(marker);
            }
          else
            {
              marker = element->querySelectors("[_child_id=" + std::to_string(child_id++) + "]");
              if (marker != nullptr)
                global_render->createPolymarker("x" + str, x_vec, "y" + str, y_vec, nullptr, 0, 0.0, 0, marker);
            }
        }
      else
        {
          if (del != del_values::update_without_default && del != del_values::update_with_default)
            {
              marker = global_render->createPolymarker("x" + str, y_vec, "y" + str, x_vec);
              marker->setAttribute("_child_id", child_id++);
              element->append(marker);
            }
          else
            {
              marker = element->querySelectors("[_child_id=" + std::to_string(child_id++) + "]");
              if (marker != nullptr)
                global_render->createPolymarker("x" + str, y_vec, "y" + str, x_vec, nullptr, 0, 0.0, 0, marker);
            }
        }

      if (!markerSizesVec.empty())
        {
          global_render->setMarkerSize(element, "markersizes" + str, markerSizesVec);
        }
      if (!markerColorIndsVec.empty())
        {
          global_render->setMarkerColorInd(element, "markercolorinds" + str, markerColorIndsVec);
        }
    }
  else
    {
      int id = static_cast<int>(global_root->getAttribute("_id"));
      std::string str = std::to_string(id);

      if (is_horizontal)
        {
          if (del != del_values::update_without_default && del != del_values::update_with_default)
            {
              marker = global_render->createPolymarker("x" + str, x_vec, "y" + str, y_vec);
              marker->setAttribute("_child_id", child_id++);
              element->append(marker);
            }
          else
            {
              marker = element->querySelectors("[_child_id=" + std::to_string(child_id++) + "]");
              if (marker != nullptr)
                global_render->createPolymarker("x" + str, x_vec, "y" + str, y_vec, nullptr, 0, 0.0, 0, marker);
            }
        }
      else
        {
          if (del != del_values::update_without_default && del != del_values::update_with_default)
            {
              marker = global_render->createPolymarker("x" + str, y_vec, "y" + str, x_vec);
              marker->setAttribute("_child_id", child_id++);
              element->append(marker);
            }
          else
            {
              marker = element->querySelectors("[_child_id=" + std::to_string(child_id++) + "]");
              if (marker != nullptr)
                global_render->createPolymarker("x" + str, y_vec, "y" + str, x_vec, nullptr, 0, 0.0, 0, marker);
            }
        }
      global_root->setAttribute("_id", ++id);
    }

  // errorbar handling
  for (const auto &child : element->children())
    {
      if (child->localName() == "errorbars") extendErrorbars(child, context, x_vec, y_vec);
    }
}

static void processScatter3(const std::shared_ptr<GRM::Element> &element, const std::shared_ptr<GRM::Context> &context)
{
  /*!
   * Processing function for scatter3
   *
   * \param[in] element The GRM::Element that contains the attributes and data keys
   * \param[in] context The GRM::Context that contains the actual data
   */
  double c_min, c_max;
  unsigned int x_length, y_length, z_length, c_length, i, c_index;
  std::vector<double> x_vec, y_vec, z_vec, c_vec;
  del_values del = del_values::update_without_default;
  int child_id = 0;
  std::shared_ptr<GRM::Element> marker;

  if (!element->hasAttribute("x")) throw NotFoundError("Scatter3 series is missing required attribute x-data.\n");
  auto x = static_cast<std::string>(element->getAttribute("x"));
  if (!element->hasAttribute("y")) throw NotFoundError("Scatter3 series is missing required attribute y-data.\n");
  auto y = static_cast<std::string>(element->getAttribute("y"));
  if (!element->hasAttribute("z")) throw NotFoundError("Scatter3 series is missing required attribute z-data.\n");
  auto z = static_cast<std::string>(element->getAttribute("z"));
  x_vec = GRM::get<std::vector<double>>((*context)[x]);
  y_vec = GRM::get<std::vector<double>>((*context)[y]);
  z_vec = GRM::get<std::vector<double>>((*context)[z]);
  x_length = x_vec.size();
  y_length = y_vec.size();
  z_length = z_vec.size();
  if (x_length != y_length || x_length != z_length)
    throw std::length_error("For scatter3 series x-, y- and z-data must have the same size.\n");

  std::vector<int> markerCVec;

  global_render->setMarkerType(element, GKS_K_MARKERTYPE_SOLID_CIRCLE);
  processMarkerType(element);
  if (element->hasAttribute("c"))
    {
      auto c = static_cast<std::string>(element->getAttribute("c"));
      c_vec = GRM::get<std::vector<double>>((*context)[c]);
      c_length = c_vec.size();
      auto plot_parent = element->parentElement();

      getPlotParent(plot_parent);
      c_min = static_cast<double>(plot_parent->getAttribute("_clim_min"));
      c_max = static_cast<double>(plot_parent->getAttribute("_clim_max"));

      for (i = 0; i < x_length; i++)
        {
          if (i < c_length)
            {
              c_index = 1000 + (int)(255.0 * (c_vec[i] - c_min) / (c_max - c_min) + 0.5);
            }
          else
            {
              c_index = 989;
            }
          markerCVec.push_back(c_index);
        }
    }

  int id_int = static_cast<int>(global_root->getAttribute("_id"));
  global_root->setAttribute("_id", ++id_int);
  std::string id = std::to_string(id_int);

  if (!markerCVec.empty())
    {
      global_render->setMarkerColorInd(element, "markercolorinds" + id, markerCVec);
    }
  else if (element->hasAttribute("c_index"))
    {
      global_render->setMarkerColorInd(element, c_index);
    }

  /* clear old marker */
  del = del_values(static_cast<int>(element->getAttribute("_delete_children")));
  clearOldChildren(&del, element);

  if (del != del_values::update_without_default && del != del_values::update_with_default)
    {
      marker = global_render->createPolymarker3d("x" + id, x_vec, "y" + id, y_vec, "z" + id, z_vec);
      marker->setAttribute("_child_id", child_id++);
      element->append(marker);
    }
  else
    {
      marker = element->querySelectors("[_child_id=" + std::to_string(child_id++) + "]");
      if (marker != nullptr)
        global_render->createPolymarker3d("x" + id, x_vec, "y" + id, y_vec, "z" + id, z_vec, nullptr, marker);
    }
}

static void processStairs(const std::shared_ptr<GRM::Element> &element, const std::shared_ptr<GRM::Context> &context)
{
  /*!
   * Processing function for stairs
   *
   * \param[in] element The GRM::Element that contains the attributes and data keys
   * \param[in] context The GRM::Context that contains the actual data
   */
  std::string kind, orientation = PLOT_DEFAULT_ORIENTATION, spec = SERIES_DEFAULT_SPEC;
  double xmin, xmax, ymin, ymax;
  int is_vertical;
  unsigned int x_length, y_length, mask, i;
  std::vector<double> x_vec, y_vec;
  std::shared_ptr<GRM::Element> element_context = element, line;
  del_values del = del_values::update_without_default;
  int child_id = 0;

  if (element->parentElement()->hasAttribute("marginalheatmap_kind")) element_context = element->parentElement();

  if (!element_context->hasAttribute("x")) throw NotFoundError("Stairs series is missing required attribute x-data.\n");
  auto x = static_cast<std::string>(element_context->getAttribute("x"));
  if (!element_context->hasAttribute("y")) throw NotFoundError("Stairs series is missing required attribute y-data.\n");
  auto y = static_cast<std::string>(element_context->getAttribute("y"));

  x_vec = GRM::get<std::vector<double>>((*context)[x]);
  y_vec = GRM::get<std::vector<double>>((*context)[y]);
  x_length = x_vec.size();
  y_length = y_vec.size();

  kind = static_cast<std::string>(element->getAttribute("kind"));
  if (element->hasAttribute("orientation"))
    {
      orientation = static_cast<std::string>(element->getAttribute("orientation"));
    }
  else
    {
      element->setAttribute("orientation", orientation);
    }
  if (element->hasAttribute("spec"))
    {
      spec = static_cast<std::string>(element->getAttribute("spec"));
    }
  else
    {
      element->setAttribute("spec", spec);
    }
  is_vertical = orientation == "vertical";

  int id = static_cast<int>(global_root->getAttribute("_id"));
  std::string str = std::to_string(id);
  global_root->setAttribute("_id", id + 1);

  /* clear old marker and lines */
  del = del_values(static_cast<int>(element->getAttribute("_delete_children")));
  clearOldChildren(&del, element);

  if (element->parentElement()->hasAttribute("marginalheatmap_kind"))
    {
      double y_max = 0;
      unsigned int z_length = 0;
      std::vector<double> z_vec;

      element->setAttribute("calc_window_and_viewport_from_parent", 1);

      auto z = static_cast<std::string>(element_context->getAttribute("z"));
      z_vec = GRM::get<std::vector<double>>((*context)[z]);
      z_length = z_vec.size();

      std::vector<double> xi_vec((is_vertical ? y_length : x_length));
      (*context)["xi" + str] = xi_vec;
      element->setAttribute("xi", "xi" + str);

      processCalcWindowAndViewportFromParent(element);
      processMarginalheatmapKind(element->parentElement());
    }
  else
    {
      if (x_length != y_length) throw std::length_error("For stairs series x- and y-data must have the same size.\n");

      const char *spec_char = spec.c_str();
      mask = gr_uselinespec((char *)spec_char);

      if (int_equals_any(mask, 5, 0, 1, 3, 4, 5))
        {
          std::string where = PLOT_DEFAULT_STEP_WHERE;
          if (element->hasAttribute("step_where"))
            {
              where = static_cast<std::string>(element->getAttribute("step_where"));
            }
          else
            {
              element->setAttribute("step_where", where);
            }
          if (where == "pre")
            {
              std::vector<double> x_step_boundaries(2 * x_length - 1);
              std::vector<double> y_step_values(2 * x_length - 1);

              x_step_boundaries[0] = x_vec[0];
              for (i = 1; i < 2 * x_length - 2; i += 2)
                {
                  x_step_boundaries[i] = x_vec[i / 2];
                  x_step_boundaries[i + 1] = x_vec[i / 2 + 1];
                }
              y_step_values[0] = y_vec[0];
              for (i = 1; i < 2 * x_length - 1; i += 2)
                {
                  y_step_values[i] = y_step_values[i + 1] = y_vec[i / 2 + 1];
                }

              std::vector<double> line_x = x_step_boundaries, line_y = y_step_values;
              if (is_vertical)
                {
                  line_x = y_step_values, line_y = x_step_boundaries;
                }
              if (del != del_values::update_without_default && del != del_values::update_with_default)
                {
                  line = global_render->createPolyline("x" + str, line_x, "y" + str, line_y);
                  line->setAttribute("_child_id", child_id++);
                  element->append(line);
                }
              else
                {
                  line = element->querySelectors("[_child_id=" + std::to_string(child_id++) + "]");
                  if (line != nullptr)
                    global_render->createPolyline("x" + str, line_x, "y" + str, line_y, nullptr, 0, 0.0, 0, line);
                }
            }
          else if (where == "post")
            {
              std::vector<double> x_step_boundaries(2 * x_length - 1);
              std::vector<double> y_step_values(2 * x_length - 1);
              for (i = 0; i < 2 * x_length - 2; i += 2)
                {
                  x_step_boundaries[i] = x_vec[i / 2];
                  x_step_boundaries[i + 1] = x_vec[i / 2 + 1];
                }
              x_step_boundaries[2 * x_length - 2] = x_vec[x_length - 1];
              for (i = 0; i < 2 * x_length - 2; i += 2)
                {
                  y_step_values[i] = y_step_values[i + 1] = y_vec[i / 2];
                }
              y_step_values[2 * x_length - 2] = y_vec[x_length - 1];

              std::vector<double> line_x = x_step_boundaries, line_y = y_step_values;
              if (is_vertical)
                {
                  line_x = y_step_values, line_y = x_step_boundaries;
                }
              if (del != del_values::update_without_default && del != del_values::update_with_default)
                {
                  line = global_render->createPolyline("x" + str, line_x, "y" + str, line_y);
                  line->setAttribute("_child_id", child_id++);
                  element->append(line);
                }
              else
                {
                  line = element->querySelectors("[_child_id=" + std::to_string(child_id++) + "]");
                  if (line != nullptr)
                    global_render->createPolyline("x" + str, line_x, "y" + str, line_y, nullptr, 0, 0.0, 0, line);
                }
            }
          else if (where == "mid")
            {
              std::vector<double> x_step_boundaries(2 * x_length);
              std::vector<double> y_step_values(2 * x_length);
              x_step_boundaries[0] = x_vec[0];
              for (i = 1; i < 2 * x_length - 2; i += 2)
                {
                  x_step_boundaries[i] = x_step_boundaries[i + 1] = (x_vec[i / 2] + x_vec[i / 2 + 1]) / 2.0;
                }
              x_step_boundaries[2 * x_length - 1] = x_vec[x_length - 1];
              for (i = 0; i < 2 * x_length - 1; i += 2)
                {
                  y_step_values[i] = y_step_values[i + 1] = y_vec[i / 2];
                }

              std::vector<double> line_x = x_step_boundaries, line_y = y_step_values;
              if (is_vertical)
                {
                  line_x = y_step_values, line_y = x_step_boundaries;
                }
              if (del != del_values::update_without_default && del != del_values::update_with_default)
                {
                  line = global_render->createPolyline("x" + str, line_x, "y" + str, line_y);
                  line->setAttribute("_child_id", child_id++);
                  element->append(line);
                }
              else
                {
                  line = element->querySelectors("[_child_id=" + std::to_string(child_id++) + "]");
                  if (line != nullptr)
                    global_render->createPolyline("x" + str, line_x, "y" + str, line_y, nullptr, 0, 0.0, 0, line);
                }
            }
        }
      if (mask & 2)
        {
          std::vector<double> line_x = x_vec, line_y = y_vec;
          if (is_vertical)
            {
              line_x = y_vec, line_y = x_vec;
            }
          if (del != del_values::update_without_default && del != del_values::update_with_default)
            {
              line = global_render->createPolyline("x" + str, line_x, "y" + str, line_y);
              line->setAttribute("_child_id", child_id++);
              element->append(line);
            }
          else
            {
              line = element->querySelectors("[_child_id=" + std::to_string(child_id++) + "]");
              if (line != nullptr)
                global_render->createPolyline("x" + str, line_x, "y" + str, line_y, nullptr, 0, 0.0, 0, line);
            }
        }
    }
}

static void processStem(const std::shared_ptr<GRM::Element> &element, const std::shared_ptr<GRM::Context> &context)
{
  /*!
   * Processing function for stem
   *
   * \param[in] element The GRM::Element that contains the attributes and data keys
   * \param[in] context The GRM::Context that contains the actual data
   */
  double stem_x[2], stem_y[2] = {0.0};
  std::string orientation = PLOT_DEFAULT_ORIENTATION, spec = SERIES_DEFAULT_SPEC;
  int is_vertical;
  double x_min, x_max, y_min, y_max;
  unsigned int x_length, y_length;
  unsigned int i;
  std::vector<double> x_vec, y_vec;
  del_values del = del_values::update_without_default;
  int child_id = 0;
  std::shared_ptr<GRM::Element> line, marker;
  double old_stem_y0;

  if (!element->hasAttribute("x")) throw NotFoundError("Stem series is missing required attribute x-data.\n");
  auto x = static_cast<std::string>(element->getAttribute("x"));
  if (!element->hasAttribute("y")) throw NotFoundError("Stem series is missing required attribute y-data.\n");
  auto y = static_cast<std::string>(element->getAttribute("y"));
  if (element->hasAttribute("orientation"))
    {
      orientation = static_cast<std::string>(element->getAttribute("orientation"));
    }
  else
    {
      element->setAttribute("orientation", orientation);
    }
  if (element->hasAttribute("spec"))
    {
      spec = static_cast<std::string>(element->getAttribute("spec"));
    }
  else
    {
      element->setAttribute("spec", spec);
    }

  is_vertical = orientation == "vertical";
  x_vec = GRM::get<std::vector<double>>((*context)[x]);
  y_vec = GRM::get<std::vector<double>>((*context)[y]);
  x_length = x_vec.size();
  y_length = y_vec.size();
  if (x_length != y_length) throw std::length_error("For stem series x- and y-data must have the same size.\n");

  if (element->hasAttribute("yrange_min")) stem_y[0] = static_cast<double>(element->getAttribute("yrange_min"));

  /* clear all old polylines and -marker */
  del = del_values(static_cast<int>(element->getAttribute("_delete_children")));
  clearOldChildren(&del, element);

  global_render->setLineSpec(element, spec);

  old_stem_y0 = stem_y[0];
  for (i = 0; i < x_length; ++i)
    {
      stem_x[0] = stem_x[1] = x_vec[i];
      stem_y[0] = old_stem_y0;
      stem_y[1] = y_vec[i];

      if (is_vertical)
        {
          double tmp1, tmp2;
          tmp1 = stem_x[0], tmp2 = stem_x[1];
          stem_x[0] = stem_y[0], stem_x[1] = stem_y[1];
          stem_y[0] = tmp1, stem_y[1] = tmp2;
        }
      if (del != del_values::update_without_default && del != del_values::update_with_default)
        {
          line = global_render->createPolyline(stem_x[0], stem_x[1], stem_y[0], stem_y[1]);
          line->setAttribute("_child_id", child_id++);
          element->append(line);
        }
      else
        {
          line = element->querySelectors("[_child_id=" + std::to_string(child_id++) + "]");
          if (line != nullptr)
            global_render->createPolyline(stem_x[0], stem_x[1], stem_y[0], stem_y[1], 0, 0.0, 0, line);
        }
    }

  int id = static_cast<int>(global_root->getAttribute("_id"));
  global_root->setAttribute("_id", id + 1);
  std::string str = std::to_string(id);

  std::vector<double> marker_x = x_vec, marker_y = y_vec;
  if (is_vertical)
    {
      marker_x = y_vec, marker_y = x_vec;
    }
  if (del != del_values::update_without_default && del != del_values::update_with_default)
    {
      marker = global_render->createPolymarker("x" + str, marker_x, "y" + str, marker_y, nullptr,
                                               GKS_K_MARKERTYPE_SOLID_CIRCLE);
      marker->setAttribute("_child_id", child_id++);
      element->append(marker);
    }
  else
    {
      marker = element->querySelectors("[_child_id=" + std::to_string(child_id++) + "]");
      if (marker != nullptr)
        global_render->createPolymarker("x" + str, marker_x, "y" + str, marker_y, nullptr,
                                        GKS_K_MARKERTYPE_SOLID_CIRCLE, 0.0, 0, marker);
    }
}

static void processShade(const std::shared_ptr<GRM::Element> &element, const std::shared_ptr<GRM::Context> &context)
{
  /* char *spec = ""; TODO: read spec from data! */
  int xform = 5, xbins = 1200, ybins = 1200, n;
  unsigned int point_count;
  unsigned int x_length, y_length;
  std::vector<double> x_vec, y_vec;
  double *x_p, *y_p;

  auto x = static_cast<std::string>(element->getAttribute("x"));
  auto y = static_cast<std::string>(element->getAttribute("y"));

  x_vec = GRM::get<std::vector<double>>((*context)[x]);
  y_vec = GRM::get<std::vector<double>>((*context)[y]);
  x_length = x_vec.size();
  y_length = y_vec.size();

  if (element->hasAttribute("xform")) xform = static_cast<int>(element->getAttribute("xform"));
  if (element->hasAttribute("xbins")) xbins = static_cast<int>(element->getAttribute("xbins"));
  if (element->hasAttribute("ybins")) ybins = static_cast<int>(element->getAttribute("ybins"));

  x_p = &(x_vec[0]);
  y_p = &(y_vec[0]);
  n = std::min<int>(x_vec.size(), y_vec.size());

  if (redrawws) gr_shadepoints(n, x_p, y_p, xform, xbins, ybins);
}

static void processSurface(const std::shared_ptr<GRM::Element> &element, const std::shared_ptr<GRM::Context> &context)
{
  /*!
   * Processing function for surface
   *
   * \param[in] element The GRM::Element that contains the attributes and data keys
   * \param[in] context The GRM::Context that contains the actual data
   */
  int accelerate = PLOT_DEFAULT_ACCELERATE; /* this argument decides if GR3 or GRM is used to plot the surface */
  std::vector<double> x_vec, y_vec, z_vec;
  unsigned int x_length, y_length, z_length;
  double x_min, x_max, y_min, y_max;

  if (element->hasAttribute("accelerate"))
    {
      accelerate = static_cast<int>(element->getAttribute("accelerate"));
    }
  else
    {
      element->setAttribute("accelerate", accelerate);
    }

  if (element->hasAttribute("x"))
    {
      auto x = static_cast<std::string>(element->getAttribute("x"));
      x_vec = GRM::get<std::vector<double>>((*context)[x]);
      x_length = x_vec.size();
    }
  if (element->hasAttribute("y"))
    {
      auto y = static_cast<std::string>(element->getAttribute("y"));
      y_vec = GRM::get<std::vector<double>>((*context)[y]);
      y_length = y_vec.size();
    }

  if (!element->hasAttribute("z")) throw NotFoundError("Surface series is missing required attribute z-data.\n");

  auto z = static_cast<std::string>(element->getAttribute("z"));
  z_vec = GRM::get<std::vector<double>>((*context)[z]);
  z_length = z_vec.size();

  if (x_vec.empty() && y_vec.empty())
    {
      /* If neither `x` nor `y` are given, we need more information about the shape of `z` */
      if (!element->hasAttribute("zdims_min") || !element->hasAttribute("zdims_max"))
        throw NotFoundError("Surface series is missing required attribute zdims.\n");
      x_length = static_cast<int>(element->getAttribute("zdims_min"));
      y_length = static_cast<int>(element->getAttribute("zdims_max"));
    }
  else if (x_vec.empty())
    {
      x_length = z_length / y_length;
    }
  else if (y_vec.empty())
    {
      y_length = z_length / x_length;
    }

  if (x_vec.empty())
    {
      x_min = static_cast<double>(element->getAttribute("xrange_min"));
      x_max = static_cast<double>(element->getAttribute("xrange_max"));
    }
  else
    {
      x_min = x_vec[0];
      x_max = x_vec[x_length - 1];
    }
  if (y_vec.empty())
    {
      y_min = static_cast<double>(element->getAttribute("yrange_min"));
      y_max = static_cast<double>(element->getAttribute("yrange_max"));
    }
  else
    {
      y_min = y_vec[0];
      y_max = y_vec[y_length - 1];
    }

  if (x_vec.empty())
    {
      std::vector<double> tmp(x_length);
      for (int j = 0; j < x_length; ++j)
        {
          tmp[j] = (int)(x_min + (x_max - x_min) / x_length * j + 0.5);
        }
      x_vec = tmp;
    }
  if (y_vec.empty())
    {
      std::vector<double> tmp(y_length);
      for (int j = 0; j < y_length; ++j)
        {
          tmp[j] = (int)(y_min + (y_max - y_min) / y_length * j + 0.5);
        }
      y_vec = tmp;
    }

  if (x_length == y_length && x_length == z_length)
    {
      logger((stderr, "Create a %d x %d grid for \"surface\" with \"gridit\"\n", PLOT_SURFACE_GRIDIT_N,
              PLOT_SURFACE_GRIDIT_N));

      std::vector<double> gridit_x_vec(PLOT_SURFACE_GRIDIT_N);
      std::vector<double> gridit_y_vec(PLOT_SURFACE_GRIDIT_N);
      std::vector<double> gridit_z_vec(PLOT_SURFACE_GRIDIT_N * PLOT_SURFACE_GRIDIT_N);

      double *gridit_x = &(gridit_x_vec[0]);
      double *gridit_y = &(gridit_y_vec[0]);
      double *gridit_z = &(gridit_z_vec[0]);
      double *x_p = &(x_vec[0]);
      double *y_p = &(y_vec[0]);
      double *z_p = &(z_vec[0]);
      gr_gridit(x_length, x_p, y_p, z_p, PLOT_SURFACE_GRIDIT_N, PLOT_SURFACE_GRIDIT_N, gridit_x, gridit_y, gridit_z);

      x_vec = std::vector<double>(gridit_x, gridit_x + PLOT_SURFACE_GRIDIT_N);
      y_vec = std::vector<double>(gridit_y, gridit_y + PLOT_SURFACE_GRIDIT_N);
      z_vec = std::vector<double>(gridit_z, gridit_z + PLOT_SURFACE_GRIDIT_N * PLOT_SURFACE_GRIDIT_N);

      x_length = y_length = PLOT_SURFACE_GRIDIT_N;
    }
  else
    {
      logger((stderr, "x_length; %u, y_length: %u, z_length: %u\n", x_length, y_length, z_length));
      if (x_length * y_length != z_length)
        throw std::length_error("For surface series x_length * y_length must be z_length.\n");
    }

  if (!accelerate)
    {
      double *px_p = &(x_vec[0]);
      double *py_p = &(y_vec[0]);
      double *pz_p = &(z_vec[0]);

      if (redrawws) gr_surface(x_length, y_length, px_p, py_p, pz_p, GR_OPTION_COLORED_MESH);
    }
  else
    {
      std::vector<float> px_vec_f(x_vec.begin(), x_vec.end());
      std::vector<float> py_vec_f(y_vec.begin(), y_vec.end());
      std::vector<float> pz_vec_f(z_vec.begin(), z_vec.end());

      float *px_p = &(px_vec_f[0]);
      float *py_p = &(py_vec_f[0]);
      float *pz_p = &(pz_vec_f[0]);

      if (redrawws) gr3_surface(x_length, y_length, px_p, py_p, pz_p, GR_OPTION_COLORED_MESH);
    }
}

static void processLine(const std::shared_ptr<GRM::Element> &element, const std::shared_ptr<GRM::Context> &context)
{
  /*!
   * Processing function for line
   *
   * \param[in] element The GRM::Element that contains the attributes and data keys
   * \param[in] context The GRM::Context that contains the actual data
   */
  std::string orientation = PLOT_DEFAULT_ORIENTATION, spec = SERIES_DEFAULT_SPEC;
  std::vector<double> x_vec, y_vec;
  unsigned int x_length = 0, y_length = 0;
  del_values del = del_values::update_without_default;
  int child_id = 0;
  std::shared_ptr<GRM::Element> line, marker;

  if (element->hasAttribute("orientation"))
    {
      orientation = static_cast<std::string>(element->getAttribute("orientation"));
    }
  else
    {
      element->setAttribute("orientation", orientation);
    }

  if (!element->hasAttribute("y")) throw NotFoundError("Line series is missing required attribute y-data.\n");
  auto y = static_cast<std::string>(element->getAttribute("y"));
  y_vec = GRM::get<std::vector<double>>((*context)[y]);
  y_length = y_vec.size();

  int mask;
  if (!element->hasAttribute("x"))
    {
      int i;
      x_length = y_length;
      for (i = 0; i < y_length; ++i) /* julia starts with 1, so GRM starts with 1 to be consistent */
        {
          x_vec.push_back(i + 1);
        }
    }
  else
    {
      auto x = static_cast<std::string>(element->getAttribute("x"));
      x_vec = GRM::get<std::vector<double>>((*context)[x]);
      x_length = x_vec.size();
    }
  if (x_length != y_length) throw std::length_error("For line series x- and y-data must have the same size.\n");

  if (element->hasAttribute("spec"))
    {
      spec = static_cast<std::string>(element->getAttribute("spec"));
    }
  else
    {
      element->setAttribute("spec", spec);
    }
  const char *spec_char = spec.c_str();
  mask = gr_uselinespec((char *)spec_char);

  /* clear old line */
  del = del_values(static_cast<int>(element->getAttribute("_delete_children")));
  clearOldChildren(&del, element);

  if (int_equals_any(mask, 5, 0, 1, 3, 4, 5))
    {
      int current_line_colorind;
      gr_inqlinecolorind(&current_line_colorind);
      int id = static_cast<int>(global_root->getAttribute("_id"));
      std::string str = std::to_string(id);

      std::vector<double> line_x = x_vec, line_y = y_vec;
      if (orientation == "vertical")
        {
          line_x = y_vec, line_y = x_vec;
        }
      if (del != del_values::update_without_default && del != del_values::update_with_default)
        {
          line = global_render->createPolyline("x" + str, line_x, "y" + str, line_y);
          line->setAttribute("_child_id", child_id++);
          element->append(line);
        }
      else
        {
          line = element->querySelectors("[_child_id=" + std::to_string(child_id++) + "]");
          if (line != nullptr)
            global_render->createPolyline("x" + str, line_x, "y" + str, line_y, nullptr, 0, 0.0, 0, line);
        }

      global_root->setAttribute("_id", ++id);
      if (line != nullptr) line->setAttribute("linecolorind", current_line_colorind);
    }
  if (mask & 2)
    {
      int current_marker_colorind;
      gr_inqmarkercolorind(&current_marker_colorind);
      int id = static_cast<int>(global_root->getAttribute("_id"));
      std::string str = std::to_string(id);

      std::vector<double> marker_x = x_vec, marker_y = y_vec;
      if (orientation == "vertical")
        {
          marker_x = y_vec, marker_y = x_vec;
        }
      if (del != del_values::update_without_default && del != del_values::update_with_default)
        {
          marker = global_render->createPolymarker("x" + str, marker_x, "y" + str, marker_y);
          marker->setAttribute("_child_id", child_id++);
          element->append(marker);
        }
      else
        {
          marker = element->querySelectors("[_child_id=" + std::to_string(child_id++) + "]");
          if (marker != nullptr)
            global_render->createPolymarker("x" + str, marker_x, "y" + str, marker_y, nullptr, 0, 0.0, 0, marker);
        }

      if (marker != nullptr)
        {
          marker->setAttribute("markercolorind", current_marker_colorind);
          marker->setAttribute("z_index", 2);

          if (element->hasAttribute("markertype"))
            {
              marker->setAttribute("markertype", static_cast<int>(element->getAttribute("markertype")));
            }
          else
            {
              marker->setAttribute("markertype", *previous_line_marker_type++);
              if (*previous_line_marker_type == INT_MAX)
                {
                  previous_line_marker_type = plot_scatter_markertypes;
                }
            }
        }
      global_root->setAttribute("_id", ++id);
    }

  // errorbar handling
  for (const auto &child : element->children())
    {
      if (child->localName() == "errorbars") extendErrorbars(child, context, x_vec, y_vec);
    }
}

static void processMarginalheatmap(const std::shared_ptr<GRM::Element> &element,
                                   const std::shared_ptr<GRM::Context> &context)
{
  /*!
   * Processing function for marginalheatmap
   *
   * \param[in] element The GRM::Element that contains the attributes and data keys
   * \param[in] context The GRM::Context that contains the actual data
   */

  double c_min, c_max;
  int flip, options;
  int xind = PLOT_DEFAULT_MARGINAL_INDEX, yind = PLOT_DEFAULT_MARGINAL_INDEX;
  unsigned int i, j, k;
  std::string algorithm = PLOT_DEFAULT_MARGINAL_ALGORITHM, marginalheatmap_kind = PLOT_DEFAULT_MARGINAL_KIND;
  std::vector<double> bins;
  unsigned int num_bins_x = 0, num_bins_y = 0, n = 0;
  std::shared_ptr<GRM::Element> subGroup;
  auto plot_parent = element->parentElement();
  del_values del = del_values::update_without_default;
  int child_id = 0;

  getPlotParent(plot_parent);

  if (element->hasAttribute("xind"))
    {
      xind = static_cast<int>(element->getAttribute("xind"));
    }
  else
    {
      element->setAttribute("xind", xind);
    }
  if (element->hasAttribute("yind"))
    {
      yind = static_cast<int>(element->getAttribute("yind"));
    }
  else
    {
      element->setAttribute("yind", yind);
    }
  if (element->hasAttribute("algorithm"))
    {
      algorithm = static_cast<std::string>(element->getAttribute("algorithm"));
    }
  else
    {
      element->setAttribute("algorithm", algorithm);
    }
  if (element->hasAttribute("marginalheatmap_kind"))
    {
      marginalheatmap_kind = static_cast<std::string>(element->getAttribute("marginalheatmap_kind"));
    }
  else
    {
      element->setAttribute("marginalheatmap_kind", marginalheatmap_kind);
    }

  auto x = static_cast<std::string>(element->getAttribute("x"));
  auto x_vec = GRM::get<std::vector<double>>((*context)[x]);
  num_bins_x = x_vec.size();

  auto y = static_cast<std::string>(element->getAttribute("y"));
  auto y_vec = GRM::get<std::vector<double>>((*context)[y]);
  num_bins_y = y_vec.size();

  auto plot = static_cast<std::string>(element->getAttribute("z"));
  auto plot_vec = GRM::get<std::vector<double>>((*context)[plot]);
  n = plot_vec.size();

  /* clear old child nodes */
  del = del_values(static_cast<int>(element->getAttribute("_delete_children")));
  clearOldChildren(&del, element);

  int id = static_cast<int>(global_root->getAttribute("_id"));
  std::string str = std::to_string(id);

  std::shared_ptr<GRM::Element> heatmap;
  if (del != del_values::update_without_default && del != del_values::update_with_default)
    {
      heatmap = global_render->createSeries("heatmap");
      heatmap->setAttribute("_child_id", child_id++);
      element->append(heatmap);
    }
  else
    {
      heatmap = element->querySelectors("[_child_id=" + std::to_string(child_id++) + "]");
    }

  // for validation
  if (heatmap != nullptr)
    {
      (*context)["z" + str] = plot_vec;
      heatmap->setAttribute("z", "z" + str);
    }

  global_root->setAttribute("_id", ++id);

  for (k = 0; k < 2; k++)
    {
      double x_min, x_max, y_min, y_max, value, bin_max = 0;
      int bar_color_index = 989;
      double bar_color_rgb[3] = {-1};
      int edge_color_index = 1;
      double edge_color_rgb[3] = {-1};

      id = static_cast<int>(global_root->getAttribute("_id"));
      str = std::to_string(id);

      x_min = static_cast<double>(element->getAttribute("xrange_min"));
      x_max = static_cast<double>(element->getAttribute("xrange_max"));
      y_min = static_cast<double>(element->getAttribute("yrange_min"));
      y_max = static_cast<double>(element->getAttribute("yrange_max"));
      if (!std::isnan(static_cast<double>(plot_parent->getAttribute("_clim_min"))))
        {
          c_min = static_cast<double>(plot_parent->getAttribute("_clim_min"));
        }
      else
        {
          c_min = static_cast<double>(plot_parent->getAttribute("_zlim_min"));
        }
      if (!std::isnan(static_cast<double>(plot_parent->getAttribute("_clim_max"))))
        {
          c_max = static_cast<double>(plot_parent->getAttribute("_clim_max"));
        }
      else
        {
          c_max = static_cast<double>(plot_parent->getAttribute("_zlim_max"));
        }

      if (marginalheatmap_kind == "all")
        {
          unsigned int x_len = num_bins_x, y_len = num_bins_y;

          bins = std::vector<double>((k == 0) ? num_bins_y : num_bins_x);

          for (i = 0; i < ((k == 0) ? num_bins_y : num_bins_x); i++)
            {
              bins[i] = 0;
            }
          for (i = 0; i < y_len; i++)
            {
              for (j = 0; j < x_len; j++)
                {
                  value = (grm_isnan(plot[i * num_bins_x + j])) ? 0 : plot_vec[i * num_bins_x + j];
                  if (algorithm == "sum")
                    {
                      bins[(k == 0) ? i : j] += value;
                    }
                  else if (algorithm == "max")
                    {
                      bins[(k == 0) ? i : j] = grm_max(bins[(k == 0) ? i : j], value);
                    }
                }
              if (k == 0)
                {
                  bin_max = grm_max(bin_max, bins[i]);
                }
            }
          if (k == 1)
            {
              for (i = 0; i < x_len; i++)
                {
                  bin_max = grm_max(bin_max, bins[i]);
                }
            }
          for (i = 0; i < ((k == 0) ? y_len : x_len); i++)
            {
              bins[i] = (bin_max == 0) ? 0 : bins[i] / bin_max * (c_max / 15);
            }

          if (del != del_values::update_without_default && del != del_values::update_with_default)
            {
              subGroup = global_render->createSeries("hist");
              subGroup->setAttribute("_child_id", child_id++);
              element->append(subGroup);
            }
          else
            {
              subGroup = element->querySelectors("[_child_id=" + std::to_string(child_id++) + "]");
            }

          if (subGroup != nullptr)
            {
              std::vector<double> bar_color_rgb_vec(bar_color_rgb, bar_color_rgb + 3);
              (*context)["bar_color_rgb" + str] = bar_color_rgb_vec;
              subGroup->setAttribute("bar_color_rgb", "bar_color_rgb" + str);
              subGroup->setAttribute("bar_color_index", bar_color_index);

              std::vector<double> edge_color_rgb_vec(edge_color_rgb, edge_color_rgb + 3);
              (*context)["edge_color_rgb" + str] = edge_color_rgb_vec;
              subGroup->setAttribute("edge_color_rgb", "edge_color_rgb" + str);
              subGroup->setAttribute("edge_color_index", edge_color_index);

              (*context)["bins" + str] = bins;
              subGroup->setAttribute("bins", "bins" + str);

              (*context)["x" + str] = x_vec;
              subGroup->setAttribute("x", "x" + str);
            }
        }
      else if (marginalheatmap_kind == "line" && xind != -1 && yind != -1)
        {
          // special case for marginalheatmap_kind line - when new indices != -1 are recieved the 2 lines should be
          // displayed
          subGroup = element->querySelectors("[_child_id=" + std::to_string(child_id) + "]");
          if ((del != del_values::update_without_default && del != del_values::update_with_default) ||
              (subGroup == nullptr && static_cast<int>(element->getAttribute("_update_required"))))
            {
              subGroup = global_render->createSeries("stairs");
              subGroup->setAttribute("_child_id", child_id++);
              element->append(subGroup);
            }
          else
            {
              child_id++;
            }

          if (subGroup != nullptr)
            {
              subGroup->setAttribute("spec", "");

              // for validation
              (*context)["y" + str] = y_vec;
              subGroup->setAttribute("y", "y" + str);

              (*context)["x" + str] = x_vec;
              subGroup->setAttribute("x", "x" + str);
            }
        }

      if (marginalheatmap_kind == "all" || (marginalheatmap_kind == "line" && xind != -1 && yind != -1))
        {
          if (subGroup != nullptr)
            {
              if (k == 0)
                {
                  subGroup->setAttribute("orientation", "vertical");
                }
              else
                {
                  subGroup->setAttribute("orientation", "horizontal");
                }
            }
        }

      for (const auto &child : element->children())
        {
          if (static_cast<std::string>(child->getAttribute("kind")) == "hist" ||
              static_cast<std::string>(child->getAttribute("kind")) == "stairs")
            {
              if (element->parentElement()->hasAttribute("xflip"))
                {
                  if (static_cast<int>(element->getAttribute("xflip")))
                    {
                      child->setAttribute("gr_option_flip_y", 1);
                      child->setAttribute("gr_option_flip_x", 0);
                    }
                }
              else if (element->parentElement()->hasAttribute("yflip"))
                {
                  if (static_cast<int>(element->getAttribute("yflip")))
                    {
                      child->setAttribute("gr_option_flip_y", 0);
                      child->setAttribute("gr_option_flip_x", 0);
                    }
                }
              else
                {
                  child->setAttribute("gr_option_flip_x", 0);
                }
            }
        }
      global_root->setAttribute("_id", ++id);
    }
}

/*!
 * \brief Set colors from color index or rgb arrays. The render version
 *
 * Call the function first with an argument container and a key. Afterwards, call the `set_next_color` with `nullptr`
 * pointers to iterate through the color arrays. If `key` does not exist in `args`, the function falls back to default
 * colors.
 *
 * \param key The key of the colors in the argument container. The key may reference integer or double arrays. Integer
 * arrays describe colors of the GKS color table (0 - 1255). Double arrays contain RGB tuples in the range [0.0, 1.0].
 * If key does not exist, the routine falls back to default colors (taken from `gr_uselinespec`). \param color_type
 * The color type to set. Can be one of `GR_COLOR_LINE`, `GR_COLOR_MARKER`, `GR_COLOR_FILL`, `GR_COLOR_TEXT`,
 * `GR_COLOR_BORDER` or any combination of them (combined with OR). The special value `GR_COLOR_RESET` resets all
 * color modifications.
 */
static int set_next_color(std::string key, gr_color_type_t color_type, const std::shared_ptr<GRM::Element> &element,
                          const std::shared_ptr<GRM::Context> &context)
{
  std::vector<int> fallback_color_indices = {989, 982, 980, 981, 996, 983, 995, 988, 986, 990,
                                             991, 984, 992, 993, 994, 987, 985, 997, 998, 999};
  static double saved_color[3];
  static int last_array_index = -1;
  static std::vector<int> color_indices;
  static std::vector<double> color_rgb_values;
  static unsigned int color_array_length = -1;
  int current_array_index = last_array_index + 1;
  int color_index = 0;
  int reset = (color_type == GR_COLOR_RESET);
  int gks_errind = GKS_K_NO_ERROR;

  if (reset || !key.empty())
    {
      if (last_array_index >= 0 && !color_rgb_values.empty())
        {
          gr_setcolorrep(PLOT_CUSTOM_COLOR_INDEX, saved_color[0], saved_color[1], saved_color[2]);
        }
      last_array_index = -1;
      if (!reset && !key.empty())
        {
          if (!element->hasAttribute("color_indices") && !element->hasAttribute("color_rgb_values"))
            {
              /* use fallback colors if `key` cannot be read from `args` */
              logger((stderr, "Cannot read \"%s\" from args, falling back to default colors\n", key.c_str()));
              color_indices = fallback_color_indices;
              color_array_length = size(fallback_color_indices);
            }
          else
            {
              if (element->hasAttribute("color_indices"))
                {
                  auto c = static_cast<std::string>(element->getAttribute("color_indices"));
                  color_indices = GRM::get<std::vector<int>>((*context)[c]);
                  color_array_length = color_indices.size();
                }
              else if (element->hasAttribute("color_rgb_values"))
                {
                  auto c = static_cast<std::string>(element->getAttribute("color_rgb_values"));
                  color_rgb_values = GRM::get<std::vector<double>>((*context)[c]);
                  color_array_length = color_rgb_values.size();
                }
            }
        }
      else
        {
          color_array_length = -1;
        }

      if (reset)
        {
          color_indices.clear();
          color_rgb_values.clear();
          return 0;
        }
      return 0;
    }

  if (last_array_index < 0 && !color_rgb_values.empty())
    {
      gks_inq_color_rep(1, PLOT_CUSTOM_COLOR_INDEX, GKS_K_VALUE_SET, &gks_errind, &saved_color[0], &saved_color[1],
                        &saved_color[2]);
    }

  current_array_index %= color_array_length;

  if (!color_indices.empty())
    {
      color_index = color_indices[current_array_index];
      last_array_index = current_array_index;
    }
  else if (!color_rgb_values.empty())
    {
      color_index = PLOT_CUSTOM_COLOR_INDEX;
      last_array_index = current_array_index + 2;
      global_render->setColorRep(element, PLOT_CUSTOM_COLOR_INDEX, color_rgb_values[current_array_index],
                                 color_rgb_values[current_array_index + 1], color_rgb_values[current_array_index + 2]);
    }

  if (color_type & GR_COLOR_LINE)
    {
      global_render->setLineColorInd(element, color_index);
    }
  if (color_type & GR_COLOR_MARKER)
    {
      global_render->setMarkerColorInd(element, color_index);
    }
  if (color_type & GR_COLOR_FILL)
    {
      global_render->setFillColorInd(element, color_index);
    }
  if (color_type & GR_COLOR_TEXT)
    {
      global_render->setTextColorInd(element, color_index);
    }
  if (color_type & GR_COLOR_BORDER)
    {
      global_render->setBorderColorInd(element, color_index);
    }
  return color_index;
}

static void processPieSegment(const std::shared_ptr<GRM::Element> &element,
                              const std::shared_ptr<GRM::Context> &context)
{
  double start_angle, end_angle, middle_angle;
  del_values del = del_values::update_without_default;
  int child_id = 0;
  std::shared_ptr<GRM::Element> arc, text_elem;
  int color_index;
  double text_pos[2];
  std::string text;

  /* clear old child nodes */
  del = del_values(static_cast<int>(element->getAttribute("_delete_children")));
  clearOldChildren(&del, element);

  start_angle = static_cast<double>(element->getAttribute("start_angle"));
  end_angle = static_cast<double>(element->getAttribute("end_angle"));
  color_index = static_cast<int>(element->getAttribute("color_ind"));
  text = static_cast<std::string>(element->getAttribute("text"));

  if (del != del_values::update_without_default && del != del_values::update_with_default)
    {
      arc = global_render->createFillArc(0.035, 0.965, 0.07, 1.0, start_angle, end_angle);
      arc->setAttribute("_child_id", child_id++);
      element->append(arc);
    }
  else
    {
      arc = element->querySelectors("[_child_id=" + std::to_string(child_id++) + "]");
      if (arc != nullptr) global_render->createFillArc(0.035, 0.965, 0.07, 1.0, start_angle, end_angle, 0, 0, -1, arc);
    }

  middle_angle = (start_angle + end_angle) / 2.0;

  text_pos[0] = 0.5 + 0.25 * cos(middle_angle * M_PI / 180.0);
  text_pos[1] = 0.5 + 0.25 * sin(middle_angle * M_PI / 180.0);

  if (del != del_values::update_without_default && del != del_values::update_with_default)
    {
      text_elem = global_render->createText(text_pos[0], text_pos[1], text, CoordinateSpace::WC);
      text_elem->setAttribute("_child_id", child_id++);
      element->append(text_elem);
    }
  else
    {
      text_elem = element->querySelectors("[_child_id=" + std::to_string(child_id++) + "]");
      if (text_elem != nullptr)
        global_render->createText(text_pos[0], text_pos[1], text, CoordinateSpace::WC, text_elem);
    }
  if (text_elem != nullptr)
    {
      text_elem->setAttribute("z_index", 2);
      text_elem->setAttribute("set_text_color_for_background", true);
      processTextColorForBackground(text_elem);
    }
}

static void processPie(const std::shared_ptr<GRM::Element> &element, const std::shared_ptr<GRM::Context> &context)
{
  /*!
   * Processing function for pie
   *
   * \param[in] element The GRM::Element that contains the attributes and data keys
   * \param[in] context The GRM::Context that contains the actual data
   */
  // TODO: color_rgb_values not needed?
  unsigned int x_length;
  int color_index;
  double start_angle, middle_angle, end_angle;
  double text_pos[2];
  char text[80];
  std::string title;
  unsigned int i;
  del_values del = del_values::update_without_default;
  int child_id = 0;
  std::shared_ptr<GRM::Element> pie_segment;

  global_render->setFillIntStyle(element, GKS_K_INTSTYLE_SOLID);
  global_render->setTextAlign(element, GKS_K_TEXT_HALIGN_CENTER, GKS_K_TEXT_VALIGN_HALF);

  if (!element->hasAttribute("x")) throw NotFoundError("Pie series is missing required attribute x-data.\n");
  auto x = static_cast<std::string>(element->getAttribute("x"));
  auto x_vec = GRM::get<std::vector<double>>((*context)[x]);
  x_length = x_vec.size();

  std::vector<double> normalized_x(x_length);
  GRM::normalize_vec(x_vec, &normalized_x);
  std::vector<unsigned int> normalized_x_int(x_length);
  GRM::normalize_vec_int(x_vec, &normalized_x_int, 1000);

  start_angle = 90;
  color_index = set_next_color("c", GR_COLOR_FILL, element, context);

  /* clear old pie_segments */
  del = del_values(static_cast<int>(element->getAttribute("_delete_children")));
  clearOldChildren(&del, element);

  for (i = 0; i < x_length; ++i)
    {
      end_angle = start_angle - normalized_x[i] * 360.0;

      snprintf(text, 80, "%.2lf\n%.1lf %%", x_vec[i], normalized_x_int[i] / 10.0);
      if (del != del_values::update_without_default && del != del_values::update_with_default)
        {
          pie_segment = global_render->createPieSegment(start_angle, end_angle, text, color_index);
          pie_segment->setAttribute("_child_id", child_id++);
          element->append(pie_segment);
        }
      else
        {
          pie_segment = element->querySelectors("[_child_id=" + std::to_string(child_id++) + "]");
          if (pie_segment != nullptr)
            global_render->createPieSegment(start_angle, end_angle, text, color_index, pie_segment);
        }
      if (pie_segment != nullptr)
        {
          color_index = set_next_color("", GR_COLOR_FILL, pie_segment, context);
          processFillColorInd(pie_segment);
        }

      start_angle = end_angle;
      if (start_angle < 0)
        {
          start_angle += 360.0;
        }
    }
  set_next_color("", GR_COLOR_RESET, element, context);
  processFillColorInd(element);
  processFillIntStyle(element);
  processTextAlign(element);
}

static void processPlot3(const std::shared_ptr<GRM::Element> &element, const std::shared_ptr<GRM::Context> &context)
{
  /*!
   * Processing function for plot3
   *
   * \param[in] element The GRM::Element that contains the attributes and data keys
   * \param[in] context The GRM::Context that contains the actual data
   */
  unsigned int x_length, y_length, z_length;
  del_values del = del_values::update_without_default;
  int child_id = 0;

  if (!element->hasAttribute("x")) throw NotFoundError("Plot3 series is missing required attribute x-data.\n");
  auto x = static_cast<std::string>(element->getAttribute("x"));
  auto x_vec = GRM::get<std::vector<double>>((*context)[x]);
  x_length = x_vec.size();

  if (!element->hasAttribute("y")) throw NotFoundError("Plot3 series is missing required attribute y-data.\n");
  auto y = static_cast<std::string>(element->getAttribute("y"));
  auto y_vec = GRM::get<std::vector<double>>((*context)[y]);
  y_length = y_vec.size();

  if (!element->hasAttribute("z")) throw NotFoundError("Plot3 series is missing required attribute z-data.\n");
  auto z = static_cast<std::string>(element->getAttribute("z"));
  auto z_vec = GRM::get<std::vector<double>>((*context)[z]);
  z_length = z_vec.size();

  if (x_length != y_length || x_length != z_length)
    throw std::length_error("For plot3 series x-, y- and z-data must have the same size.\n");

  /* clear old line */
  del = del_values(static_cast<int>(element->getAttribute("_delete_children")));
  clearOldChildren(&del, element);

  int id_int = static_cast<int>(global_root->getAttribute("_id"));
  global_root->setAttribute("_id", ++id_int);
  std::string id = std::to_string(id_int);

  std::shared_ptr<GRM::Element> line;
  if (del != del_values::update_without_default && del != del_values::update_with_default)
    {
      line = global_render->createPolyline3d("x" + id, x_vec, "y" + id, y_vec, "z" + id, z_vec);
      line->setAttribute("_child_id", child_id++);
      element->append(line);
    }
  else
    {
      line = element->querySelectors("[_child_id=" + std::to_string(child_id++) + "]");
      if (line != nullptr)
        global_render->createPolyline3d("x" + id, x_vec, "y" + id, y_vec, "z" + id, z_vec, nullptr, line);
    }
}

static void processImshow(const std::shared_ptr<GRM::Element> &element, const std::shared_ptr<GRM::Context> &context)
{
  /*!
   * Processing function for imshow
   *
   * \param[in] element The GRM::Element that contains the attributes and data keys
   * \param[in] context The GRM::Context that contains the actual data
   */
  double c_min, c_max;
  unsigned int c_data_length, i, j, k;
  int grplot = 0;
  int rows, cols;
  auto plot_parent = element->parentElement();
  del_values del = del_values::update_without_default;
  int child_id = 0;

  getPlotParent(plot_parent);
  if (plot_parent->hasAttribute("grplot"))
    {
      grplot = static_cast<int>(plot_parent->getAttribute("grplot"));
    }
  if (std::isnan(static_cast<double>(plot_parent->getAttribute("_clim_min"))))
    throw NotFoundError("Imshow series is missing required attribute clim.\n");
  c_min = static_cast<double>(plot_parent->getAttribute("_clim_min"));
  if (std::isnan(static_cast<double>(plot_parent->getAttribute("_clim_max"))))
    throw NotFoundError("Imshow series is missing required attribute clim.\n");
  c_max = static_cast<double>(plot_parent->getAttribute("_clim_max"));
  logger((stderr, "Got min, max %lf %lf\n", c_min, c_max));

  std::vector<double> c_data_vec, shape_vec;

  if (!element->hasAttribute("c")) throw NotFoundError("Imshow series is missing required attribute c-data.\n");
  auto x = static_cast<std::string>(element->getAttribute("c"));
  if (!element->hasAttribute("c_dims"))
    throw NotFoundError("Imshow series is missing required attribute c_dims-data.\n");
  auto y = static_cast<std::string>(element->getAttribute("c_dims"));

  c_data_vec = GRM::get<std::vector<double>>((*context)[x]);
  shape_vec = GRM::get<std::vector<double>>((*context)[y]);
  c_data_length = c_data_vec.size();
  i = shape_vec.size();
  if (i != 2) throw std::length_error("The size of shape data from imshow has to be 2.\n");
  if (shape_vec[0] * shape_vec[1] != c_data_length)
    throw std::length_error("For imshow shape[0] * shape[1] must be c_data_length.\n");

  cols = (int)shape_vec[0];
  rows = (int)shape_vec[1];

  std::vector<int> img_data(c_data_length);

  k = 0;
  for (j = 0; j < rows; ++j)
    {
      for (i = 0; i < cols; ++i)
        {
          img_data[k++] = 1000 + (int)grm_round((1.0 * c_data_vec[j * cols + i] - c_min) / (c_max - c_min) * 255);
        }
    }

  int id_int = static_cast<int>(global_root->getAttribute("_id"));
  global_root->setAttribute("_id", ++id_int);
  std::string id = std::to_string(id_int);

  std::vector<double> x_vec, y_vec;
  linspace(0, cols - 1, cols, x_vec);
  linspace(0, rows - 1, rows, y_vec);

  (*context)["x" + id] = x_vec;
  element->setAttribute("x", "x" + id);
  (*context)["y" + id] = y_vec;
  element->setAttribute("y", "y" + id);
  (*context)["z" + id] = c_data_vec;
  element->setAttribute("z", "z" + id);
  (*context)["img_data_key" + id] = img_data;
  element->setAttribute("img_data_key", "img_data_key" + id);

  global_render->setSelntran(element, 0);
  global_render->setScale(element, 0);
  GRM::Render::processScale(element);
  processSelntran(element);

  double x_min, x_max, y_min, y_max;
  double vp[4];
  int scale;
  std::string image_data_key = static_cast<std::string>(element->getAttribute("img_data_key"));
  std::shared_ptr<GRM::Element> ancestor = element->parentElement();
  bool vp_found = false;

  // Get vp from ancestor GRM::element, usually the q"plot-group"
  while (ancestor->localName() != "figure")
    {
      if (ancestor->hasAttribute("vp_xmin") && ancestor->hasAttribute("vp_xmax") && ancestor->hasAttribute("vp_ymin") &&
          ancestor->hasAttribute("vp_ymax"))
        {
          vp[0] = static_cast<double>(ancestor->getAttribute("vp_xmin"));
          vp[1] = static_cast<double>(ancestor->getAttribute("vp_xmax"));
          vp[2] = static_cast<double>(ancestor->getAttribute("vp_ymin"));
          vp[3] = static_cast<double>(ancestor->getAttribute("vp_ymax"));
          vp_found = true;
          break;
        }
      else
        {
          ancestor = ancestor->parentElement();
        }
    }
  if (!vp_found)
    {
      throw NotFoundError("No vp was found within ancestors\n");
    }

  gr_inqscale(&scale);

  if (cols * (vp[3] - vp[2]) < rows * (vp[1] - vp[0]))
    {
      double w = (double)cols / (double)rows * (vp[3] - vp[2]);
      x_min = grm_max(0.5 * (vp[0] + vp[1] - w), vp[0]);
      x_max = grm_min(0.5 * (vp[0] + vp[1] + w), vp[1]);
      y_min = vp[2];
      y_max = vp[3];
    }
  else
    {
      double h = (double)rows / (double)cols * (vp[1] - vp[0]);
      x_min = vp[0];
      x_max = vp[1];
      y_min = grm_max(0.5 * (vp[3] + vp[2] - h), vp[2]);
      y_max = grm_min(0.5 * (vp[3] + vp[2] + h), vp[3]);
    }

  if (scale & GR_OPTION_FLIP_X)
    {
      double tmp = x_max;
      x_max = x_min;
      x_min = tmp;
    }
  if (scale & GR_OPTION_FLIP_Y)
    {
      double tmp = y_max;
      y_max = y_min;
      y_min = tmp;
    }
  if (grplot)
    {
      double tmp = y_min;
      y_min = y_max;
      y_max = tmp;
    }

  /* remove old cell arrays if they exist */
  del = del_values(static_cast<int>(element->getAttribute("_delete_children")));
  if (del != del_values::update_without_default && del != del_values::update_with_default)
    {
      for (const auto &child : element->children())
        {
          if (del == del_values::recreate_own_children)
            {
              if (child->hasAttribute("_child_id")) child->remove();
            }
          else if (del == del_values::recreate_all_children)
            {
              child->remove();
            }
        }
    }
  else if (!element->hasChildNodes())
    del = del_values::recreate_own_children;

  std::shared_ptr<GRM::Element> cellarray;
  if (del != del_values::update_without_default && del != del_values::update_with_default)
    {
      cellarray = global_render->createCellArray(x_min, x_max, y_min, y_max, cols, rows, 1, 1, cols, rows,
                                                 image_data_key, std::nullopt);
      cellarray->setAttribute("_child_id", child_id++);
      element->append(cellarray);
    }
  else
    {
      cellarray = element->querySelectors("[_child_id=" + std::to_string(child_id++) + "]");
      if (cellarray != nullptr)
        global_render->createCellArray(x_min, x_max, y_min, y_max, cols, rows, 1, 1, cols, rows, image_data_key,
                                       std::nullopt, nullptr, cellarray);
    }
  if (cellarray != nullptr) cellarray->setAttribute("name", "imshow");
}

static void processText(const std::shared_ptr<GRM::Element> &element, const std::shared_ptr<GRM::Context> &context)
{
  /*!
   * Processing function for text
   *
   * \param[in] element The GRM::Element that contains the attributes and data keys
   * \param[in] context The GRM::Context that contains the actual data
   */
  gr_savestate();
  auto x = static_cast<double>(element->getAttribute("x"));
  auto y = static_cast<double>(element->getAttribute("y"));
  auto str = static_cast<std::string>(element->getAttribute("text"));
  auto available_width = static_cast<double>(element->getAttribute("width"));
  auto available_height = static_cast<double>(element->getAttribute("height"));
  double tbx[4], tby[4];
  bool text_fits = true;
  CoordinateSpace space = static_cast<CoordinateSpace>(static_cast<int>(element->getAttribute("space")));

  if (space == CoordinateSpace::WC)
    {
      gr_wctondc(&x, &y);
    }
  if (element->hasAttribute("width") && element->hasAttribute("height"))
    {
      gr_wctondc(&available_width, &available_height);
      gr_inqtext(x, y, &str[0], tbx, tby);
      auto minmax_x = std::minmax_element(std::begin(tbx), std::end(tbx));
      auto minmax_y = std::minmax_element(std::begin(tby), std::end(tby));
      double width = minmax_x.second - minmax_x.first;
      double height = minmax_y.second - minmax_y.first;
      if (width > available_width && height > available_height)
        {
          gr_setcharup(0.0, 1.0);
          gr_settextalign(2, 3);
          gr_inqtext(x, y, &str[0], tbx, tby);
          width = tbx[2] - tbx[0];
          height = tby[2] - tby[0];
          if (width < available_width && height < available_height)
            {
              gr_setcharup(0.0, 1.0);
              gr_settextalign(2, 3);
            }
          else if (height < available_width && width < available_height)
            {
              gr_setcharup(-1.0, 0.0);
              gr_settextalign(2, 3);
            }
          else
            {
              text_fits = false;
            }
        }
    }
  if (text_fits && redrawws) gr_text(x, y, &str[0]);
  gr_restorestate();
}

static void processTitles3d(const std::shared_ptr<GRM::Element> &element, const std::shared_ptr<GRM::Context> &context)
{
  /*!
   * Processing function for titles3d
   *
   * \param[in] element The GRM::Element that contains the attributes and data keys
   * \param[in] context The GRM::Context that contains the actual data
   */
  std::string x, y, z;
  x = static_cast<std::string>(element->getAttribute("x"));
  y = static_cast<std::string>(element->getAttribute("y"));
  z = static_cast<std::string>(element->getAttribute("z"));
  if (redrawws) gr_titles3d(x.data(), y.data(), z.data());
}

static void processTriContour(const std::shared_ptr<GRM::Element> &element,
                              const std::shared_ptr<GRM::Context> &context)
{
  /*!
   * Processing function for tricontour
   *
   * \param[in] element The GRM::Element that contains the attributes and data keys
   * \param[in] context The GRM::Context that contains the actual data
   */
  double z_min, z_max;
  int num_levels = PLOT_DEFAULT_TRICONT_LEVELS;
  int i;
  unsigned int x_length, y_length, z_length;
  std::vector<double> x_vec, y_vec, z_vec;
  auto plot_parent = element->parentElement();

  getPlotParent(plot_parent);
  z_min = static_cast<double>(plot_parent->getAttribute("_zlim_min"));
  z_max = static_cast<double>(plot_parent->getAttribute("_zlim_max"));
  if (element->hasAttribute("levels"))
    {
      num_levels = static_cast<int>(element->getAttribute("levels"));
    }
  else
    {
      element->setAttribute("levels", num_levels);
    }

  std::vector<double> levels(num_levels);

  for (i = 0; i < num_levels; ++i)
    {
      levels[i] = z_min + ((1.0 * i) / (num_levels - 1)) * (z_max - z_min);
    }

  if (!element->hasAttribute("x")) throw NotFoundError("Tricontour series is missing required attribute px-data.\n");
  auto x = static_cast<std::string>(element->getAttribute("x"));
  if (!element->hasAttribute("y")) throw NotFoundError("Tricontour series is missing required attribute py-data.\n");
  auto y = static_cast<std::string>(element->getAttribute("y"));
  if (!element->hasAttribute("z")) throw NotFoundError("Tricontour series is missing required attribute pz-data.\n");
  auto z = static_cast<std::string>(element->getAttribute("z"));

  x_vec = GRM::get<std::vector<double>>((*context)[x]);
  y_vec = GRM::get<std::vector<double>>((*context)[y]);
  z_vec = GRM::get<std::vector<double>>((*context)[z]);
  x_length = x_vec.size();
  y_length = y_vec.size();
  z_length = z_vec.size();

  if (x_length != y_length || x_length != z_length)
    throw std::length_error("For tricontour series x-, y- and z-data must have the same size.\n");

  double *px_p = &(x_vec[0]);
  double *py_p = &(y_vec[0]);
  double *pz_p = &(z_vec[0]);
  double *l_p = &(levels[0]);

  if (redrawws) gr_tricontour(x_length, px_p, py_p, pz_p, num_levels, l_p);
}

static void processTriSurface(const std::shared_ptr<GRM::Element> &element,
                              const std::shared_ptr<GRM::Context> &context)
{
  /*!
   * Processing function for trisurface
   *
   * \param[in] element The GRM::Element that contains the attributes and data keys
   * \param[in] context The GRM::Context that contains the actual data
   */
  if (!element->hasAttribute("x")) throw NotFoundError("Trisurface series is missing required attribute px-data.\n");
  auto px = static_cast<std::string>(element->getAttribute("x"));
  if (!element->hasAttribute("y")) throw NotFoundError("Trisurface series is missing required attribute py-data.\n");
  auto py = static_cast<std::string>(element->getAttribute("y"));
  if (!element->hasAttribute("z")) throw NotFoundError("Trisurface series is missing required attribute pz-data.\n");
  auto pz = static_cast<std::string>(element->getAttribute("z"));

  std::vector<double> px_vec = GRM::get<std::vector<double>>((*context)[px]);
  std::vector<double> py_vec = GRM::get<std::vector<double>>((*context)[py]);
  std::vector<double> pz_vec = GRM::get<std::vector<double>>((*context)[pz]);

  int nx = px_vec.size();
  int ny = py_vec.size();
  int nz = pz_vec.size();
  if (nx != ny || nx != nz)
    throw std::length_error("For trisurface series px-, py- and pz-data must have the same size.\n");

  double *px_p = &(px_vec[0]);
  double *py_p = &(py_vec[0]);
  double *pz_p = &(pz_vec[0]);

  if (redrawws) gr_trisurface(nx, px_p, py_p, pz_p);
}

static void volume(const std::shared_ptr<GRM::Element> &element, const std::shared_ptr<GRM::Context> &context)
{
  int width, height;
  double device_pixel_ratio;
  double dmin = -1, dmax = -1;

  auto c = static_cast<std::string>(element->getAttribute("c"));
  auto c_vec = GRM::get<std::vector<double>>((*context)[c]);
  auto c_dims = static_cast<std::string>(element->getAttribute("c_dims"));
  auto shape_vec = GRM::get<std::vector<int>>((*context)[c_dims]);
  int algorithm = getVolumeAlgorithm(element);
  if (element->hasAttribute("dmin")) dmin = static_cast<double>(element->getAttribute("dmin"));
  if (element->hasAttribute("dmax")) dmax = static_cast<double>(element->getAttribute("dmax"));

  gr_inqvpsize(&width, &height, &device_pixel_ratio);
  gr_setpicturesizeforvolume((int)(width * device_pixel_ratio), (int)(height * device_pixel_ratio));
  if (element->hasAttribute("_volume_context_address"))
    {
      auto address = static_cast<std::string>(element->getAttribute("_volume_context_address"));
      long volume_address = stol(address, 0, 16);
      const gr3_volume_2pass_t *volume_context = (gr3_volume_2pass_t *)volume_address;
      if (redrawws)
        gr_volume_2pass(shape_vec[0], shape_vec[1], shape_vec[2], &(c_vec[0]), algorithm, &dmin, &dmax, volume_context);
      element->removeAttribute("_volume_context_address");
    }
  else
    {
      if (redrawws) gr_volume(shape_vec[0], shape_vec[1], shape_vec[2], &(c_vec[0]), algorithm, &dmin, &dmax);
    }
}

static void processVolume(const std::shared_ptr<GRM::Element> &element, const std::shared_ptr<GRM::Context> &context)
{
  double dlim[2] = {INFINITY, -INFINITY};
  unsigned int c_length, dims;
  int algorithm = PLOT_DEFAULT_VOLUME_ALGORITHM;
  std::string algorithm_str;
  double dmin, dmax;
  int width, height;
  double device_pixel_ratio;

  if (!element->hasAttribute("c")) throw NotFoundError("Volume series is missing required attribute c-data.\n");
  auto c = static_cast<std::string>(element->getAttribute("c"));
  auto c_vec = GRM::get<std::vector<double>>((*context)[c]);
  c_length = c_vec.size();

  if (!element->hasAttribute("c_dims")) throw NotFoundError("Volume series is missing required attribute c_dims.\n");
  auto c_dims = static_cast<std::string>(element->getAttribute("c_dims"));
  auto shape_vec = GRM::get<std::vector<int>>((*context)[c_dims]);
  dims = shape_vec.size();

  if (dims != 3) throw std::length_error("For volume series the size of c_dims has to be 3.\n");
  if (shape_vec[0] * shape_vec[1] * shape_vec[2] != c_length)
    throw std::length_error("For volume series shape[0] * shape[1] * shape[2] must be c_length.\n");
  if (c_length <= 0) throw NotFoundError("For volume series the size of c has to be greater than 0.\n");

  if (!element->hasAttribute("algorithm"))
    {
      element->setAttribute("algorithm", algorithm);
    }
  else
    {
      algorithm = getVolumeAlgorithm(element);
    }
  if (algorithm != GR_VOLUME_ABSORPTION && algorithm != GR_VOLUME_EMISSION && algorithm != GR_VOLUME_MIP)
    {
      logger((stderr, "Got unknown volume algorithm \"%d\"\n", algorithm));
      throw std::logic_error("For volume series the given algorithm is unknown.\n");
    }

  dmin = dmax = -1.0;
  if (element->hasAttribute("dmin")) dmin = static_cast<double>(element->getAttribute("dmin"));
  if (element->hasAttribute("dmax")) dmax = static_cast<double>(element->getAttribute("dmax"));

  gr_inqvpsize(&width, &height, &device_pixel_ratio);
  gr_setpicturesizeforvolume((int)(width * device_pixel_ratio), (int)(height * device_pixel_ratio));
  if (redrawws)
    {
      const gr3_volume_2pass_t *volumeContext =
          gr_volume_2pass(shape_vec[0], shape_vec[1], shape_vec[2], &(c_vec[0]), algorithm, &dmin, &dmax, nullptr);

      std::ostringstream get_address;
      get_address << volumeContext;
      element->setAttribute("_volume_context_address", get_address.str());

      auto parent_element = element->parentElement();
      if (parent_element->hasAttribute("clim_min") && parent_element->hasAttribute("clim_max"))
        {
          dlim[0] = static_cast<double>(parent_element->getAttribute("clim_min"));
          dlim[1] = static_cast<double>(parent_element->getAttribute("clim_max"));
          dlim[0] = grm_min(dlim[0], dmin);
          dlim[1] = grm_max(dlim[1], dmax);
        }
      else
        {
          dlim[0] = dmin;
          dlim[1] = dmax;
        }

      auto colorbar = parent_element->querySelectors("colorbar");
      parent_element->setAttribute("_clim_min", dlim[0]);
      parent_element->setAttribute("_clim_max", dlim[1]);
      PushDrawableToZQueue pushVolumeToZQueue(volume);
      pushVolumeToZQueue(element, context);
    }
}

static void processWireframe(const std::shared_ptr<GRM::Element> &element, const std::shared_ptr<GRM::Context> &context)
{
  /*!
   * Processing function for wireframe
   *
   * \param[in] element The GRM::Element that contains the attributes and data keys
   * \param[in] context The GRM::Context that contains the actual data
   */
  unsigned int x_length, y_length, z_length;

  auto x = static_cast<std::string>(element->getAttribute("x"));
  auto y = static_cast<std::string>(element->getAttribute("y"));
  auto z = static_cast<std::string>(element->getAttribute("z"));

  std::vector<double> x_vec = GRM::get<std::vector<double>>((*context)[x]);
  std::vector<double> y_vec = GRM::get<std::vector<double>>((*context)[y]);
  std::vector<double> z_vec = GRM::get<std::vector<double>>((*context)[z]);
  x_length = x_vec.size();
  y_length = y_vec.size();
  z_length = z_vec.size();

  global_render->setFillColorInd(element, 0);
  processFillColorInd(element);

  int id_int = static_cast<int>(global_root->getAttribute("_id"));
  global_root->setAttribute("_id", ++id_int);
  std::string id = std::to_string(id_int);

  if (x_length == y_length && x_length == z_length)
    {
      std::vector<double> gridit_x_vec(PLOT_WIREFRAME_GRIDIT_N);
      std::vector<double> gridit_y_vec(PLOT_WIREFRAME_GRIDIT_N);
      std::vector<double> gridit_z_vec(PLOT_WIREFRAME_GRIDIT_N * PLOT_WIREFRAME_GRIDIT_N);

      double *gridit_x = &(gridit_x_vec[0]);
      double *gridit_y = &(gridit_y_vec[0]);
      double *gridit_z = &(gridit_z_vec[0]);
      double *x_p = &(x_vec[0]);
      double *y_p = &(y_vec[0]);
      double *z_p = &(z_vec[0]);

      gr_gridit(x_length, x_p, y_p, z_p, PLOT_WIREFRAME_GRIDIT_N, PLOT_WIREFRAME_GRIDIT_N, gridit_x, gridit_y,
                gridit_z);

      x_vec = std::vector<double>(gridit_x, gridit_x + PLOT_WIREFRAME_GRIDIT_N);
      y_vec = std::vector<double>(gridit_y, gridit_y + PLOT_WIREFRAME_GRIDIT_N);
      z_vec = std::vector<double>(gridit_z, gridit_z + PLOT_WIREFRAME_GRIDIT_N * PLOT_WIREFRAME_GRIDIT_N);
    }
  else
    {
      if (x_length * y_length != z_length)
        throw std::length_error("For wireframe series x_length * y_length must be z_length.\n");
    }

  double *px_p = &(x_vec[0]);
  double *py_p = &(y_vec[0]);
  double *pz_p = &(z_vec[0]);
  if (redrawws) gr_surface(x_length, y_length, px_p, py_p, pz_p, GR_OPTION_FILLED_MESH);
}

void plotProcessWswindowWsviewport(const std::shared_ptr<GRM::Element> &element,
                                   const std::shared_ptr<GRM::Context> &context)
{
  int pixel_width, pixel_height;
  double metric_width, metric_height;
  double aspect_ratio_ws_pixel, aspect_ratio_ws_metric;
  double wsviewport[4] = {0.0, 0.0, 0.0, 0.0};
  double wswindow[4] = {0.0, 0.0, 0.0, 0.0};

  // set wswindow/wsviewport on active figure
  GRM::Render::get_figure_size(&pixel_width, &pixel_height, &metric_width, &metric_height);

  if (!active_figure->hasAttribute("_previous_pixel_width") || !active_figure->hasAttribute("_previous_pixel_height") ||
      (static_cast<int>(active_figure->getAttribute("_previous_pixel_width")) != pixel_width ||
       static_cast<int>(active_figure->getAttribute("_previous_pixel_height")) != pixel_height))
    {
      /* TODO: handle error return value? */
      event_queue_enqueue_size_event(event_queue, static_cast<int>(active_figure->getAttribute("figure_id")),
                                     pixel_width, pixel_height);
    }

  aspect_ratio_ws_pixel = (double)pixel_width / pixel_height;
  aspect_ratio_ws_metric = metric_width / metric_height;
  if (aspect_ratio_ws_pixel > 1)
    {
      wsviewport[1] = metric_width;
      wsviewport[3] = metric_width / aspect_ratio_ws_metric;
      wswindow[1] = 1.0;
      wswindow[3] = 1.0 / (aspect_ratio_ws_pixel);
    }
  else
    {
      wsviewport[1] = metric_height * aspect_ratio_ws_metric;
      wsviewport[3] = metric_height;
      wswindow[1] = aspect_ratio_ws_pixel;
      wswindow[3] = 1.0;
    }
  global_render->setWSViewport(active_figure, wsviewport[0], wsviewport[1], wsviewport[2], wsviewport[3]);
  global_render->setWSWindow(active_figure, wswindow[0], wswindow[1], wswindow[2], wswindow[3]);

  active_figure->setAttribute("_previous_pixel_width", pixel_width);
  active_figure->setAttribute("_previous_pixel_height", pixel_height);

  logger((stderr, "Stored wswindow (%lf, %lf, %lf, %lf)\n", wswindow[0], wswindow[1], wswindow[2], wswindow[3]));
  logger(
      (stderr, "Stored wsviewport (%lf, %lf, %lf, %lf)\n", wsviewport[0], wsviewport[1], wsviewport[2], wsviewport[3]));
}

static void plotCoordinateRanges(const std::shared_ptr<GRM::Element> &element,
                                 const std::shared_ptr<GRM::Context> &context)
{
  std::string kind, style;
  const char *fmt;
  unsigned int series_count = 0;
  std::vector<std::string> data_component_names = {"x", "y", "z", "c", ""};
  std::vector<std::string>::iterator current_component_name;
  std::vector<double> current_component;
  unsigned int current_point_count = 0;
  struct
  {
    const char *subplot;
    const char *series;
  } * current_range_keys,
      range_keys[] = {{"xlim", "xrange"}, {"ylim", "yrange"}, {"zlim", "zrange"}, {"clim", "crange"}};
  std::vector<double> bins;
  unsigned int i;

  logger((stderr, "Storing coordinate ranges\n"));

  /* If a pan and/or zoom was performed before, do not overwrite limits
   * -> the user fully controls limits by interaction */
  if (element->hasAttribute("original_xlim"))
    {
      logger((stderr, "Panzoom active, do not modify limits...\n"));
    }
  else
    {
      element->setAttribute("_xlim_min", NAN);
      element->setAttribute("_xlim_max", NAN);
      element->setAttribute("_ylim_min", NAN);
      element->setAttribute("_ylim_max", NAN);
      element->setAttribute("_zlim_min", NAN);
      element->setAttribute("_zlim_max", NAN);
      element->setAttribute("_clim_min", NAN);
      element->setAttribute("_clim_max", NAN);
      kind = static_cast<std::string>(element->getAttribute("kind"));
      if (!string_map_at(fmt_map, static_cast<const char *>(kind.c_str()), static_cast<const char **>(&fmt)))
        throw NotFoundError("Invalid kind was given.\n");
      if (!str_equals_any(kind.c_str(), 2, "pie", "polar_histogram"))
        {
          current_component_name = data_component_names.begin();
          current_range_keys = range_keys;
          while (!(*current_component_name).empty())
            {
              double min_component = DBL_MAX;
              double max_component = -DBL_MAX;
              double step = -DBL_MAX;

              if (static_cast<std::string>(fmt).find(*current_component_name) == std::string::npos)
                {
                  ++current_range_keys;
                  ++current_component_name;
                  continue;
                }
              /* Heatmaps need calculated range keys, so run the calculation even if limits are given */
              if (!element->hasAttribute(static_cast<std::string>(current_range_keys->subplot) + "_min") ||
                  !element->hasAttribute(static_cast<std::string>(current_range_keys->subplot) + "_max") ||
                  str_equals_any(kind.c_str(), 3, "heatmap", "marginalheatmap", "polar_heatmap"))
                {
                  for (const auto &series : element->children())
                    {
                      if (!starts_with(series->localName(), "series")) continue;
                      if (series->hasAttribute("style"))
                        style = static_cast<std::string>(series->getAttribute("style"));
                      double current_min_component = DBL_MAX, current_max_component = -DBL_MAX;
                      if (!series->hasAttribute(static_cast<std::string>(current_range_keys->series) + "_min") ||
                          !series->hasAttribute((static_cast<std::string>(current_range_keys->series) + "_max")))
                        {
                          if (series->hasAttribute(*current_component_name))
                            {
                              auto key = static_cast<std::string>(series->getAttribute(*current_component_name));
                              current_component = GRM::get<std::vector<double>>((*context)[key]);
                              current_point_count = current_component.size();
                              if (style == "stacked")
                                {
                                  current_max_component = 0.0;
                                  current_min_component = 0.0;
                                  for (i = 0; i < current_point_count; i++)
                                    {
                                      if (current_component[i] > 0)
                                        {
                                          current_max_component += current_component[i];
                                        }
                                      else
                                        {
                                          current_min_component += current_component[i];
                                        }
                                    }
                                }
                              else
                                {
                                  if (kind == "barplot")
                                    {
                                      current_min_component = 0.0;
                                      current_max_component = 0.0;
                                    }
                                  for (i = 0; i < current_point_count; i++)
                                    {
                                      if (!std::isnan(current_component[i]))
                                        {
                                          current_min_component = grm_min(current_component[i], current_min_component);
                                          current_max_component = grm_max(current_component[i], current_max_component);
                                        }
                                    }
                                }
                            }
                          /* TODO: Add more plot types which can omit `x` */
                          else if (kind == "line" && *current_component_name == "x")
                            {
                              unsigned int y_length;
                              if (!series->hasAttribute("y"))
                                throw NotFoundError("Series is missing required attribute y.\n");
                              auto key = static_cast<std::string>(series->getAttribute("y"));
                              auto y_vec = GRM::get<std::vector<double>>((*context)[key]);
                              y_length = y_vec.size();
                              current_min_component = 0.0;
                              current_max_component = y_length - 1;
                            }
                          else if (str_equals_any(kind.c_str(), 4, "heatmap", "marginalheatmap", "polar_heatmap",
                                                  "surface") &&
                                   str_equals_any((*current_component_name).c_str(), 2, "x", "y"))
                            {
                              /* in this case `x` or `y` (or both) are missing
                               * -> set the current grm_min/max_component to the dimensions of `z`
                               *    (shifted by half a unit to center color blocks) */
                              const char *other_component_name = (*current_component_name == "x") ? "y" : "x";
                              std::vector<double> other_component;
                              unsigned int other_point_count;
                              if (series->hasAttribute(other_component_name))
                                {
                                  /* The other component is given -> the missing dimension can be calculated */
                                  unsigned int z_length;

                                  auto key = static_cast<std::string>(series->getAttribute(other_component_name));
                                  other_component = GRM::get<std::vector<double>>((*context)[key]);
                                  other_point_count = other_component.size();

                                  if (!series->hasAttribute("z"))
                                    throw NotFoundError("Series is missing required attribute z.\n");
                                  auto z_key = static_cast<std::string>(series->getAttribute("z"));
                                  auto z_vec = GRM::get<std::vector<double>>((*context)[z_key]);
                                  z_length = z_vec.size();
                                  current_point_count = z_length / other_point_count;
                                }
                              else
                                {
                                  /* A heatmap/surface without `x` and `y` values
                                   * -> dimensions can only be read from `z_dims` */
                                  int rows, cols;
                                  if (!series->hasAttribute("zdims_min") || !series->hasAttribute("zdims_max"))
                                    throw NotFoundError("Series is missing attribute zdims.\n");
                                  rows = static_cast<int>(series->getAttribute("zdims_min"));
                                  cols = static_cast<int>(series->getAttribute("zdims_max"));
                                  current_point_count = (*current_component_name == "x") ? cols : rows;
                                }
                              current_min_component = 0.5;
                              current_max_component = current_point_count + 0.5;
                            }
                          else if (series->hasAttribute("indices"))
                            {
                              auto indices_key = static_cast<std::string>(series->getAttribute("indices"));
                              auto indices = GRM::get<std::vector<int>>((*context)[indices_key]);

                              if (series->hasAttribute(*current_component_name))
                                {
                                  int index_sum = 0;
                                  auto key = static_cast<std::string>(series->getAttribute(*current_component_name));
                                  current_component = GRM::get<std::vector<double>>((*context)[key]);
                                  current_point_count = current_component.size();

                                  current_max_component = 0;
                                  current_min_component = 0;
                                  auto act_index = indices.begin();
                                  index_sum += *act_index;
                                  for (i = 0; i < current_point_count; i++)
                                    {
                                      if (!std::isnan(current_component[i]))
                                        {
                                          if (current_component[i] > 0)
                                            {
                                              current_max_component += current_component[i];
                                            }
                                          else
                                            {
                                              current_min_component += current_component[i];
                                            }
                                        }
                                      if (i + 1 == index_sum)
                                        {
                                          max_component = grm_max(current_max_component, max_component);
                                          min_component = grm_min(current_min_component, min_component);

                                          current_max_component = 0;
                                          current_min_component = 0;
                                          ++act_index;
                                          index_sum += *act_index;
                                        }
                                    }
                                }
                            }
                        }
                      else
                        {
                          current_min_component = static_cast<double>(
                              series->getAttribute(static_cast<std::string>(current_range_keys->series) + "_min"));
                          current_max_component = static_cast<double>(
                              series->getAttribute(static_cast<std::string>(current_range_keys->series) + "_max"));
                        }

                      if (current_min_component != DBL_MAX && current_max_component != -DBL_MAX)
                        {
                          if (static_cast<std::string>(current_range_keys->series) == "xrange")
                            {
                              series->setAttribute("xrange_min", current_min_component);
                              series->setAttribute("xrange_max", current_max_component);
                            }
                          else if (static_cast<std::string>(current_range_keys->series) == "yrange")
                            {
                              series->setAttribute("yrange_min", current_min_component);
                              series->setAttribute("yrange_max", current_max_component);
                            }
                          else if (static_cast<std::string>(current_range_keys->series) == "zrange")
                            {
                              series->setAttribute("zrange_min", current_min_component);
                              series->setAttribute("zrange_max", current_max_component);
                            }
                          else if (static_cast<std::string>(current_range_keys->series) == "crange")
                            {
                              series->setAttribute("crange_min", current_min_component);
                              series->setAttribute("crange_max", current_max_component);
                            }
                        }
                      min_component = grm_min(current_min_component, min_component);
                      max_component = grm_max(current_max_component, max_component);
                      series_count++;
                    }
                }
              if (element->hasAttribute(static_cast<std::string>(current_range_keys->subplot) + "_min") &&
                  element->hasAttribute(static_cast<std::string>(current_range_keys->subplot) + "_max"))
                {
                  min_component = static_cast<double>(
                      element->getAttribute(static_cast<std::string>(current_range_keys->subplot) + "_min"));
                  max_component = static_cast<double>(
                      element->getAttribute(static_cast<std::string>(current_range_keys->subplot) + "_max"));
                  if (static_cast<std::string>(current_range_keys->subplot) == "xlim")
                    {
                      element->setAttribute("_xlim_min", min_component);
                      element->setAttribute("_xlim_max", max_component);
                    }
                  else if (static_cast<std::string>(current_range_keys->subplot) == "ylim")
                    {
                      element->setAttribute("_ylim_min", min_component);
                      element->setAttribute("_ylim_max", max_component);
                    }
                  else if (static_cast<std::string>(current_range_keys->subplot) == "zlim")
                    {
                      element->setAttribute("_zlim_min", min_component);
                      element->setAttribute("_zlim_max", max_component);
                    }
                  else if (static_cast<std::string>(current_range_keys->subplot) == "clim")
                    {
                      element->setAttribute("_clim_min", min_component);
                      element->setAttribute("_clim_max", max_component);
                    }
                }
              else if (min_component != DBL_MAX && max_component != -DBL_MAX)
                {
                  if (kind == "quiver")
                    {
                      step = grm_max(find_max_step(current_point_count, current_component), step);
                      if (step > 0.0)
                        {
                          min_component -= step;
                          max_component += step;
                        }
                    }
                  // TODO: Support mixed orientations
                  std::string orientation = PLOT_DEFAULT_ORIENTATION;
                  for (const auto &series : element->children())
                    {
                      if (series->hasAttribute("orientation"))
                        orientation = static_cast<std::string>(series->getAttribute("orientation"));
                      if (!starts_with(series->localName(), "series")) continue;
                    }
                  if (static_cast<std::string>(current_range_keys->subplot) == "xlim")
                    {
                      if (orientation == "horizontal")
                        {
                          element->setAttribute("_xlim_min", min_component);
                          element->setAttribute("_xlim_max", max_component);
                        }
                      else
                        {
                          element->setAttribute("_ylim_min", min_component);
                          element->setAttribute("_ylim_max", max_component);
                        }
                    }
                  else if (static_cast<std::string>(current_range_keys->subplot) == "ylim")
                    {
                      if (orientation == "horizontal")
                        {
                          element->setAttribute("_ylim_min", min_component);
                          element->setAttribute("_ylim_max", max_component);
                        }
                      else
                        {
                          element->setAttribute("_xlim_min", min_component);
                          element->setAttribute("_xlim_max", max_component);
                        }
                    }
                  else if (static_cast<std::string>(current_range_keys->subplot) == "zlim")
                    {
                      element->setAttribute("_zlim_min", min_component);
                      element->setAttribute("_zlim_max", max_component);
                    }
                  else if (static_cast<std::string>(current_range_keys->subplot) == "clim")
                    {
                      element->setAttribute("_clim_min", min_component);
                      element->setAttribute("_clim_max", max_component);
                    }
                }
              ++current_range_keys;
              ++current_component_name;
            }
        }
      else if (kind == "polar_histogram")
        {
          element->setAttribute("_xlim_min", -1.0);
          element->setAttribute("_xlim_max", 1.0);
          element->setAttribute("_ylim_min", -1.0);
          element->setAttribute("_ylim_max", 1.0);
        }

      /* For quiver plots use u^2 + v^2 as z value */
      if (kind == "quiver")
        {
          double min_component = DBL_MAX;
          double max_component = -DBL_MAX;
          if (!element->hasAttribute("zlim_min") || !element->hasAttribute("zlim_max"))
            {
              for (const auto &series : element->children())
                {
                  if (!starts_with(series->localName(), "series")) continue;
                  double current_min_component = DBL_MAX;
                  double current_max_component = -DBL_MAX;
                  if (!series->hasAttribute("zrange_min") || !series->hasAttribute("zrange_max"))
                    {
                      unsigned int u_length, v_length;
                      if (!series->hasAttribute("u"))
                        throw NotFoundError("Quiver series is missing required attribute u-data.\n");
                      auto u_key = static_cast<std::string>(series->getAttribute("u"));
                      if (!series->hasAttribute("v"))
                        throw NotFoundError("Quiver series is missing required attribute v-data.\n");
                      auto v_key = static_cast<std::string>(series->getAttribute("v"));

                      std::vector<double> u = GRM::get<std::vector<double>>((*context)[u_key]);
                      std::vector<double> v = GRM::get<std::vector<double>>((*context)[v_key]);
                      u_length = u.size();
                      v_length = v.size();
                      if (u_length != v_length)
                        throw std::length_error("For quiver series the shape of u and v must be the same.\n");

                      for (i = 0; i < u_length; i++)
                        {
                          double z = u[i] * u[i] + v[i] * v[i];
                          current_min_component = grm_min(z, current_min_component);
                          current_max_component = grm_max(z, current_max_component);
                        }
                      current_min_component = sqrt(current_min_component);
                      current_max_component = sqrt(current_max_component);
                    }
                  else
                    {
                      current_min_component = static_cast<double>(series->getAttribute("zrange_min"));
                      current_max_component = static_cast<double>(series->getAttribute("zrange_max"));
                    }
                  min_component = grm_min(current_min_component, min_component);
                  max_component = grm_max(current_max_component, max_component);
                }
            }
          else
            {
              min_component = static_cast<double>(element->getAttribute("zlim_min"));
              max_component = static_cast<double>(element->getAttribute("zlim_max"));
            }
          element->setAttribute("_zlim_min", min_component);
          element->setAttribute("_zlim_max", max_component);
        }
      else if (str_equals_any(kind.c_str(), 3, "imshow", "isosurface", "volume"))
        {
          /* Iterate over `x` and `y` range keys (and `z` depending on `kind`) */
          current_range_keys = range_keys;
          for (i = 0; i < (kind == "imshow" ? 2 : 3); i++)
            {
              double min_component = (kind == "imshow" ? 0.0 : -1.0);
              double max_component = 1.0;
              if (element->hasAttribute(static_cast<std::string>(current_range_keys->subplot) + "_min") &&
                  element->hasAttribute(static_cast<std::string>(current_range_keys->subplot) + "_max"))
                {
                  min_component = static_cast<double>(
                      element->getAttribute(static_cast<std::string>(current_range_keys->subplot) + "_min"));
                  max_component = static_cast<double>(
                      element->getAttribute(static_cast<std::string>(current_range_keys->subplot) + "_max"));
                }
              if (static_cast<std::string>(current_range_keys->subplot) == "xlim")
                {
                  element->setAttribute("_xlim_min", min_component);
                  element->setAttribute("_xlim_max", max_component);
                }
              else if (static_cast<std::string>(current_range_keys->subplot) == "ylim")
                {
                  element->setAttribute("_ylim_min", min_component);
                  element->setAttribute("_ylim_max", max_component);
                }
              else if (static_cast<std::string>(current_range_keys->subplot) == "zlim")
                {
                  element->setAttribute("_zlim_min", min_component);
                  element->setAttribute("_zlim_max", max_component);
                }
              else if (static_cast<std::string>(current_range_keys->subplot) == "clim")
                {
                  element->setAttribute("_clim_min", min_component);
                  element->setAttribute("_clim_max", max_component);
                }
              ++current_range_keys;
            }
        }
      else if (kind == "barplot")
        {
          double x_min = 0.0, x_max = -DBL_MAX, y_min = DBL_MAX, y_max = -DBL_MAX;
          std::string orientation = PLOT_DEFAULT_ORIENTATION;

          if (!element->hasAttribute("xlim_min") || !element->hasAttribute("xlim_max"))
            {
              double xmin, xmax;

              for (const auto &series : element->children())
                {
                  if (!starts_with(series->localName(), "series")) continue;
                  if (series->hasAttribute("style")) style = static_cast<std::string>(series->getAttribute("style"));
                  if (str_equals_any(style.c_str(), 2, "lined", "stacked"))
                    {
                      x_max = series_count + 1;
                    }
                  else
                    {
                      auto key = static_cast<std::string>(series->getAttribute("y"));
                      auto y = GRM::get<std::vector<double>>((*context)[key]);
                      current_point_count = y.size();
                      x_max = grm_max(current_point_count + 1, x_max);
                    }
                }

              for (const auto &series : element->children())
                {
                  if (!starts_with(series->localName(), "series")) continue;
                  if (series->hasAttribute("orientation"))
                    orientation = static_cast<std::string>(series->getAttribute("orientation"));
                  auto key = static_cast<std::string>(series->getAttribute("y"));
                  auto y = GRM::get<std::vector<double>>((*context)[key]);
                  current_point_count = y.size();

                  if (series->hasAttribute("xrange_min") && series->hasAttribute("xrange_max"))
                    {
                      xmin = static_cast<double>(series->getAttribute("xrange_min"));
                      xmax = static_cast<double>(series->getAttribute("xrange_max"));
                      double step_x = (xmax - xmin) / (current_point_count - 1);
                      if (!str_equals_any(style.c_str(), 2, "lined", "stacked"))
                        {
                          x_min = xmin - step_x;
                          x_max = xmax + step_x;
                        }
                      else
                        {
                          x_min = xmin - (x_max - 1);
                          x_max = xmin + (x_max - 1);
                        }
                    }
                }
            }
          else
            {
              x_min = static_cast<double>(element->getAttribute("xlim_min"));
              x_max = static_cast<double>(element->getAttribute("xlim_max"));
            }

          if (!element->hasAttribute("ylim_min") || !element->hasAttribute("ylim_max"))
            {
              double ymin, ymax;
              for (const auto &series : element->children())
                {
                  if (!starts_with(series->localName(), "series")) continue;
                  if (series->hasAttribute("style")) style = static_cast<std::string>(series->getAttribute("style"));
                  if (series->hasAttribute("orientation"))
                    orientation = static_cast<std::string>(series->getAttribute("orientation"));
                  auto key = static_cast<std::string>(series->getAttribute("y"));
                  auto y = GRM::get<std::vector<double>>((*context)[key]);
                  current_point_count = y.size();

                  if (series->hasAttribute("yrange_min") && series->hasAttribute("yrange_max"))
                    {
                      ymin = static_cast<double>(series->getAttribute("yrange_min"));
                      ymax = static_cast<double>(series->getAttribute("yrange_max"));
                      y_min = grm_min(y_min, ymin);
                      if (style == "stacked")
                        {
                          double tmp_ymax;
                          tmp_ymax = ymin;
                          for (i = 0; i < current_point_count; i++)
                            {
                              if (y_min < 0)
                                {
                                  tmp_ymax += fabs(y[i]);
                                }
                              else
                                {
                                  tmp_ymax += y[i] - y_min;
                                }
                            }
                          y_max = grm_max(y_max, tmp_ymax);
                        }
                      else
                        {
                          y_max = grm_max(y_max, ymax);
                        }
                    }
                }
            }
          else
            {
              y_min = static_cast<double>(element->getAttribute("ylim_min"));
              y_max = static_cast<double>(element->getAttribute("ylim_max"));
            }

          if (orientation == "horizontal")
            {
              element->setAttribute("_xlim_min", x_min);
              element->setAttribute("_xlim_max", x_max);
              element->setAttribute("_ylim_min", y_min);
              element->setAttribute("_ylim_max", y_max);
            }
          else
            {
              element->setAttribute("_xlim_min", y_min);
              element->setAttribute("_xlim_max", y_max);
              element->setAttribute("_ylim_min", x_min);
              element->setAttribute("_ylim_max", x_max);
            }
        }
      else if (kind == "hist")
        {
          double x_min = 0.0, x_max = 0.0, y_min = 0.0, y_max = 0.0;
          std::string orientation = PLOT_DEFAULT_ORIENTATION;
          int is_horizontal;

          if (!element->hasAttribute("ylim_min") || !element->hasAttribute("ylim_max"))
            {
              for (const auto &series : element->children())
                {
                  if (series->hasAttribute("orientation"))
                    orientation = static_cast<std::string>(series->getAttribute("orientation"));
                  is_horizontal = orientation == "horizontal";
                  if (!starts_with(series->localName(), "series")) continue;
                  double current_y_min = DBL_MAX, current_y_max = -DBL_MAX;

                  if (!series->hasAttribute("bins")) histBins(series, context);
                  auto bins_key = static_cast<std::string>(series->getAttribute("bins"));
                  bins = GRM::get<std::vector<double>>((*context)[bins_key]);
                  int num_bins = bins.size();

                  for (i = 0; i < num_bins; i++)
                    {
                      current_y_min = grm_min(current_y_min, bins[i]);
                      current_y_max = grm_max(current_y_max, bins[i]);
                    }

                  y_min = grm_min(current_y_min, y_min);
                  y_max = grm_max(current_y_max, y_max);
                  x_max = current_point_count - 1;
                  if (series->hasAttribute("yrange_min") && series->hasAttribute("yrange_max"))
                    {
                      y_min = static_cast<double>(series->getAttribute("yrange_min"));
                      y_max = static_cast<double>(series->getAttribute("yrange_max"));
                    }
                  if (series->hasAttribute("xrange_min") && series->hasAttribute("xrange_max"))
                    {
                      x_min = static_cast<double>(series->getAttribute("xrange_min"));
                      x_max = static_cast<double>(series->getAttribute("xrange_max"));
                    }
                }
              if (is_horizontal)
                {
                  element->setAttribute("_xlim_min", x_min);
                  element->setAttribute("_xlim_max", x_max);
                  element->setAttribute("_ylim_min", y_min);
                  element->setAttribute("_ylim_max", y_max);
                }
              else
                {
                  element->setAttribute("_xlim_min", y_min);
                  element->setAttribute("_xlim_max", y_max);
                  element->setAttribute("_ylim_min", x_min);
                  element->setAttribute("_ylim_max", x_max);
                }
            }
          else
            {
              y_min = static_cast<double>(element->getAttribute("ylim_min"));
              y_max = static_cast<double>(element->getAttribute("ylim_max"));
            }
        }
      else if (str_equals_any(kind.c_str(), 2, "stem", "stairs"))
        {
          double x_min = 0.0, x_max = 0.0, y_min = 0.0, y_max = 0.0;
          std::string orientation = PLOT_DEFAULT_ORIENTATION;
          int is_horizontal;

          for (const auto &series : element->children())
            {
              if (series->hasAttribute("orientation"))
                orientation = static_cast<std::string>(series->getAttribute("orientation"));
              is_horizontal = orientation == "horizontal";
              if (!starts_with(series->localName(), "series")) continue;
              if (series->hasAttribute("xrange_min") && series->hasAttribute("xrange_max") &&
                  !(element->hasAttribute("xlim_min") && element->hasAttribute("xlim_max")))
                {
                  x_min = static_cast<double>(series->getAttribute("xrange_min"));
                  x_max = static_cast<double>(series->getAttribute("xrange_max"));
                  if (is_horizontal)
                    {
                      element->setAttribute("_xlim_min", x_min);
                      element->setAttribute("_xlim_max", x_max);
                    }
                  else
                    {
                      element->setAttribute("_ylim_min", x_min);
                      element->setAttribute("_ylim_max", x_max);
                    }
                }
              if (series->hasAttribute("yrange_min") && series->hasAttribute("yrange_max") &&
                  !(element->hasAttribute("ylim_min") && element->hasAttribute("ylim_max")))
                {
                  y_min = static_cast<double>(series->getAttribute("yrange_min"));
                  y_max = static_cast<double>(series->getAttribute("yrange_max"));
                  if (is_horizontal)
                    {
                      element->setAttribute("_ylim_min", y_min);
                      element->setAttribute("_ylim_max", y_max);
                    }
                  else
                    {
                      element->setAttribute("_xlim_min", y_min);
                      element->setAttribute("_xlim_max", y_max);
                    }
                }
            }
        }
    }
}

static void processCoordinateSystem(const std::shared_ptr<GRM::Element> &element,
                                    const std::shared_ptr<GRM::Context> &context)
{
  for (const auto &parentchild : element->parentElement()->children())
    {
      if (parentchild->localName() == "series_barplot" || parentchild->localName() == "series_stem")
        {
          auto kind = static_cast<std::string>(parentchild->getAttribute("kind"));
          if (kind == "barplot" || kind == "stem")
            {
              if (!element->hasAttribute("yline")) element->setAttribute("yline", true);
            }
          break;
        }
    }
  /* 0-line */
  if (element->hasAttribute("yline") && static_cast<int>(element->getAttribute("yline"))) drawYLine(element, context);
}

static void processPlot(const std::shared_ptr<GRM::Element> &element, const std::shared_ptr<GRM::Context> &context)
{
  if (!element->hasAttribute("_xlim_min") || !element->hasAttribute("_xlim_max") ||
      !element->hasAttribute("_ylim_min") || !element->hasAttribute("_ylim_max"))
    plotCoordinateRanges(element, context);
  processSubplot(element);
  GRM::Render::processViewport(element);
  // todo: there are cases that element does not have charheight set
  // charheight is always calculated (and set in the gr) in processViewport
  // it is however not stored on the element as it can be calculated from other attributes
  if (element->hasAttribute("charheight"))
    {
      processCharHeight(element);
    }
  GRM::Render::processLimits(element);
  GRM::Render::processWindow(element); /* needs to be set before space3d is processed */
  GRM::Render::processScale(element);  /* needs to be set before flip is processed */

  /* Map for calculations on the plot level */
  static std::map<std::string,
                  std::function<void(const std::shared_ptr<GRM::Element> &, const std::shared_ptr<GRM::Context> &)>>
      kindNameToFunc{
          {std::string("barplot"), preBarplot},
          {std::string("polar_histogram"), prePolarHistogram},
      };

  for (const auto &child : element->children())
    {
      if (child->localName() == "series_barplot" || child->localName() == "series_polar_histogram")
        {
          auto kind = static_cast<std::string>(child->getAttribute("kind"));
          if (kindNameToFunc.find(kind) != kindNameToFunc.end())
            {
              kindNameToFunc[kind](element, context);
            }
          break;
        }
    }
}

static void processSeries(const std::shared_ptr<GRM::Element> &element, const std::shared_ptr<GRM::Context> &context)
{
  static std::map<std::string,
                  std::function<void(const std::shared_ptr<GRM::Element>, const std::shared_ptr<GRM::Context>)>>
      seriesNameToFunc{
          {std::string("barplot"), processBarplot},
          {std::string("contour"), PushDrawableToZQueue(processContour)},
          {std::string("contourf"), PushDrawableToZQueue(processContourf)},
          {std::string("heatmap"), processHeatmap},
          {std::string("hexbin"), processHexbin},
          {std::string("hist"), processHist},
          {std::string("imshow"), processImshow},
          {std::string("isosurface"), PushDrawableToZQueue(processIsosurface)},
          {std::string("line"), processLine},
          {std::string("marginalheatmap"), processMarginalheatmap},
          {std::string("pie"), processPie},
          {std::string("plot3"), processPlot3},
          {std::string("polar"), processPolar},
          {std::string("polar_heatmap"), processPolarHeatmap},
          {std::string("polar_histogram"), processPolarHistogram},
          {std::string("quiver"), PushDrawableToZQueue(processQuiver)},
          {std::string("scatter"), processScatter},
          {std::string("scatter3"), processScatter3},
          {std::string("shade"), PushDrawableToZQueue(processShade)},
          {std::string("stairs"), processStairs},
          {std::string("stem"), processStem},
          {std::string("surface"), PushDrawableToZQueue(processSurface)},
          {std::string("tricontour"), PushDrawableToZQueue(processTriContour)},
          {std::string("trisurface"), PushDrawableToZQueue(processTriSurface)},
          {std::string("volume"), processVolume},
          {std::string("wireframe"), PushDrawableToZQueue(processWireframe)},
      };

  auto kind = static_cast<std::string>(element->getAttribute("kind"));

  if (auto search = seriesNameToFunc.find(kind); search != seriesNameToFunc.end())
    {
      auto f = search->second;
      f(element, context);
    }
  else
    {
      throw NotFoundError("Series is not in render implemented yet\n");
    }

  // special case where the data of a series could inflict the window
  // its important that the series gets first processed so the changed data gets used inside plotCoordinateRanges
  if (element->parentElement()->localName() == "plot" &&
      !static_cast<int>(element->parentElement()->getAttribute("keep_window")))
    {
      plotCoordinateRanges(element->parentElement(), global_render->getContext());
    }
}

static void processElement(const std::shared_ptr<GRM::Element> &element, const std::shared_ptr<GRM::Context> &context)
{
  /*!
   * Processing function for all kinds of elements
   *
   * \param[in] element The GRM::Element containing attributes and data keys
   * \param[in] context The GRM::Context containing the actual data
   */

  //! Map used for processing all kinds of elements
  bool update_required = static_cast<int>(element->getAttribute("_update_required"));
  static std::map<std::string,
                  std::function<void(const std::shared_ptr<GRM::Element>, const std::shared_ptr<GRM::Context>)>>
      elemStringToFunc{
          {std::string("axes"), processAxes},
          {std::string("axes3d"), processAxes3d},
          {std::string("bar"), processBar},
          {std::string("cellarray"), PushDrawableToZQueue(processCellArray)},
          {std::string("colorbar"), processColorbar},
          {std::string("coordinate_system"), processCoordinateSystem},
          {std::string("errorbar"), processErrorbar},
          {std::string("errorbars"), processErrorbars},
          {std::string("legend"), processLegend},
          {std::string("polar_axes"), processPolarAxes},
          {std::string("drawarc"), PushDrawableToZQueue(processDrawArc)},
          {std::string("drawgraphics"), PushDrawableToZQueue(processDrawGraphics)},
          {std::string("drawimage"), PushDrawableToZQueue(processDrawImage)},
          {std::string("drawrect"), PushDrawableToZQueue(processDrawRect)},
          {std::string("fillarc"), PushDrawableToZQueue(processFillArc)},
          {std::string("fillarea"), PushDrawableToZQueue(processFillArea)},
          {std::string("fillrect"), PushDrawableToZQueue(processFillRect)},
          {std::string("gr3clear"), PushDrawableToZQueue(processGr3Clear)},
          {std::string("gr3deletemesh"), PushDrawableToZQueue(processGr3DeleteMesh)},
          {std::string("gr3drawimage"), PushDrawableToZQueue(processGr3DrawImage)},
          {std::string("gr3drawmesh"), PushDrawableToZQueue(processGr3DrawMesh)},
          {std::string("grid"), PushDrawableToZQueue(processGrid)},
          {std::string("grid3d"), PushDrawableToZQueue(processGrid3d)},
          {std::string("isosurface_render"), PushDrawableToZQueue(processIsosurfaceRender)},
          {std::string("layout_grid"), PushDrawableToZQueue(processLayoutGrid)},
          {std::string("layout_gridelement"), PushDrawableToZQueue(processLayoutGridElement)},
          {std::string("nonuniform_polarcellarray"), PushDrawableToZQueue(processNonUniformPolarCellArray)},
          {std::string("nonuniformcellarray"), PushDrawableToZQueue(processNonuniformcellarray)},
          {std::string("panzoom"), PushDrawableToZQueue(processPanzoom)},
          {std::string("pie_segment"), processPieSegment},
          {std::string("polar_bar"), processPolarBar},
          {std::string("polarcellarray"), PushDrawableToZQueue(processPolarCellArray)},
          {std::string("polyline"), PushDrawableToZQueue(processPolyline)},
          {std::string("polyline3d"), PushDrawableToZQueue(processPolyline3d)},
          {std::string("polymarker"), PushDrawableToZQueue(processPolymarker)},
          {std::string("polymarker3d"), PushDrawableToZQueue(processPolymarker3d)},
          {std::string("series"), processSeries},
          {std::string("text"), PushDrawableToZQueue(processText)},
          {std::string("titles3d"), PushDrawableToZQueue(processTitles3d)},
      };

  /*! Modifier */
  if (str_equals_any(element->localName().c_str(), 8, "axes_text_group", "figure", "plot", "label", "labels_group",
                     "root", "xticklabel_group", "yticklabel_group"))
    {
      bool old_state = automatic_update;
      automatic_update = false;
      /* check if figure is active; skip inactive elements */
      if (element->localName() == "figure")
        {
          if (!static_cast<int>(element->getAttribute("active"))) return;
          if (global_root->querySelectorsAll("drawgraphics").empty()) plotProcessWswindowWsviewport(element, context);
        }
      if (element->localName() == "plot") processPlot(element, context);
      GRM::Render::processAttributes(element);
      automatic_update = old_state;
    }
  else
    {
      // TODO: something like series_contour shouldn't be in this list
      if (!automatic_update ||
          ((static_cast<int>(global_root->getAttribute("_modified")) &&
            (str_equals_any(element->localName().c_str(), 33, "axes", "axes3d", "cellarray", "colorbar", "drawarc",
                            "drawimage", "drawrect", "fillarc", "fillarea", "fillrect", "grid", "grid3d", "legend",
                            "nonuniform_polarcellarray", "nonuniformcellarray", "polarcellarray", "polyline",
                            "polyline3d", "polymarker", "polymarker3d", "series_contour", "series_contourf", "text",
                            "titles3d", "coordinate_system", "series_hexbin", "series_isosurface", "series_quiver",
                            "series_shade", "series_surface", "series_tricontour", "series_trisurface",
                            "series_volume") ||
             !element->hasChildNodes())) ||
           update_required))
        {
          // elements without children are the draw-functions which need to be processed everytime, else there could
          // be problems with overlapping elements
          std::string local_name = getLocalName(element);

          bool old_state = automatic_update;
          automatic_update = false;
          /* The attributes of drawables (except for the z_index itself) are being processed when the z_queue is being
           * processed */
          if (isDrawable(element))
            {
              if (element->hasAttribute("z_index")) processZIndex(element);
            }
          else
            {
              GRM::Render::processAttributes(element);
            }

          if (auto search = elemStringToFunc.find(local_name); search != elemStringToFunc.end())
            {
              auto f = search->second;
              f(element, context);
            }
          else
            {
              throw NotFoundError("No dom render function found for element with local name: " + element->localName() +
                                  "\n");
            }

          // reset _update_required
          element->setAttribute("_update_required", false);
          element->setAttribute("_delete_children", 0);
          automatic_update = old_state;
        }
      else if (automatic_update && static_cast<int>(global_root->getAttribute("_modified")) ||
               element->hasAttribute("calc_window_and_viewport_from_parent"))
        {
          bool old_state = automatic_update;
          automatic_update = false;
          GRM::Render::processAttributes(element);
          automatic_update = old_state;
        }
    }
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~ render functions ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 */

static void renderHelper(const std::shared_ptr<GRM::Element> &element, const std::shared_ptr<GRM::Context> &context)
{
  /*!
   * Recursive helper function for render; Not part of render class
   * Only renders / processes children if the parent is in parentTypes (group etc.)
   * Used for traversing the tree
   *
   * \param[in] element A GRM::Element
   * \param[in] context A GRM::Context
   */
  gr_savestate();
  zIndexManager.savestate();
  customColorIndexManager.savestate();

  bool bounding_boxes = getenv("GRPLOT_ENABLE_EDITOR");

  if (bounding_boxes && !isDrawable(element))
    {
      gr_begin_grm_selection(bounding_id, &receiverfunction);
      bounding_map[bounding_id] = element;
      bounding_id++;
    }

  processElement(element, context);
  if (element->hasChildNodes() && parentTypes.count(element->localName()))
    {
      for (const auto &child : element->children())
        {
          if (child->localName() == "figure" && !static_cast<int>(child->getAttribute("active"))) continue;
          renderHelper(child, context);
        }
    }
  if (bounding_boxes && !isDrawable(element))
    {
      gr_end_grm_selection();
    }

  customColorIndexManager.restorestate();
  zIndexManager.restorestate();
  gr_restorestate();
}

static void missing_bbox_calculator(const std::shared_ptr<GRM::Element> &element,
                                    const std::shared_ptr<GRM::Context> &context, double *bbox_xmin = nullptr,
                                    double *bbox_xmax = nullptr, double *bbox_ymin = nullptr,
                                    double *bbox_ymax = nullptr)
{
  double elem_bbox_xmin = DBL_MAX, elem_bbox_xmax = -DBL_MAX, elem_bbox_ymin = DBL_MAX, elem_bbox_ymax = -DBL_MAX;

  if (element->hasAttribute("_bbox_id") && static_cast<int>(element->getAttribute("_bbox_id")) != -1)
    {
      *bbox_xmin = static_cast<double>(element->getAttribute("_bbox_xmin"));
      *bbox_xmax = static_cast<double>(element->getAttribute("_bbox_xmax"));
      *bbox_ymin = static_cast<double>(element->getAttribute("_bbox_ymin"));
      *bbox_ymax = static_cast<double>(element->getAttribute("_bbox_ymax"));
    }
  else
    {
      if (element->hasChildNodes() && parentTypes.count(element->localName()))
        {
          for (const auto &child : element->children())
            {
              double tmp_bbox_xmin = DBL_MAX, tmp_bbox_xmax = -DBL_MAX, tmp_bbox_ymin = DBL_MAX,
                     tmp_bbox_ymax = -DBL_MAX;
              missing_bbox_calculator(child, context, &tmp_bbox_xmin, &tmp_bbox_xmax, &tmp_bbox_ymin, &tmp_bbox_ymax);
              elem_bbox_xmin = grm_min(elem_bbox_xmin, tmp_bbox_xmin);
              elem_bbox_xmax = grm_max(elem_bbox_xmax, tmp_bbox_xmax);
              elem_bbox_ymin = grm_min(elem_bbox_ymin, tmp_bbox_ymin);
              elem_bbox_ymax = grm_max(elem_bbox_ymax, tmp_bbox_ymax);
            }
        }
    }

  if (element->localName() != "root" &&
      (!element->hasAttribute("_bbox_id") || static_cast<int>(element->getAttribute("_bbox_id")) == -1))
    {
      if (!(elem_bbox_xmin == DBL_MAX || elem_bbox_xmax == -DBL_MAX || elem_bbox_ymin == DBL_MAX ||
            elem_bbox_ymax == -DBL_MAX))
        {
          if (static_cast<int>(element->getAttribute("_bbox_id")) != -1)
            {
              element->setAttribute("_bbox_id", bounding_id++);
            }
          element->setAttribute("_bbox_xmin", elem_bbox_xmin);
          element->setAttribute("_bbox_xmax", elem_bbox_xmax);
          element->setAttribute("_bbox_ymin", elem_bbox_ymin);
          element->setAttribute("_bbox_ymax", elem_bbox_ymax);
        }

      if (bbox_xmin != nullptr) *bbox_xmin = elem_bbox_xmin;
      if (bbox_xmax != nullptr) *bbox_xmax = elem_bbox_xmax;
      if (bbox_ymin != nullptr) *bbox_ymin = elem_bbox_ymin;
      if (bbox_ymax != nullptr) *bbox_ymax = elem_bbox_ymax;
    }
}

static void renderZQueue(const std::shared_ptr<GRM::Context> &context)
{
  zQueueIsBeingRendered = true;
  bool bounding_boxes = getenv("GRPLOT_ENABLE_EDITOR");

  gr_savestate();
  for (; !z_queue.empty(); z_queue.pop())
    {
      auto drawable = z_queue.top();
      auto element = drawable->getElement();
      if (!element->parentElement()) continue;

      if (bounding_boxes)
        {
          gr_begin_grm_selection(bounding_id, &receiverfunction);
          bounding_map[bounding_id] = element;
          bounding_id++;
        }

      customColorIndexManager.selectcontext(drawable->getGrContextId());
      drawable->draw();

      if (bounding_boxes)
        {
          gr_end_grm_selection();
        }
    }
  grContextIDManager.markAllIdsAsUnused();
  parent_to_context = {};
  gr_unselectcontext();
  gr_restorestate();
  zQueueIsBeingRendered = false;
}

static void initializeGridElements(const std::shared_ptr<GRM::Element> &element, grm::Grid *grid)
{
  if (element->hasChildNodes())
    {
      for (const auto &child : element->children())
        {
          if (child->localName() != "layout_gridelement" && child->localName() != "layout_grid")
            {
              return;
            }

          double absHeight = static_cast<double>(child->getAttribute("absHeight"));
          double absWidth = static_cast<double>(child->getAttribute("absWidth"));
          int absHeightPxl = static_cast<int>(child->getAttribute("absHeightPxl"));
          int absWidthPxl = static_cast<int>(child->getAttribute("absWidthPxl"));
          int fitParentsHeight = static_cast<int>(child->getAttribute("fitParentsHeight"));
          int fitParentsWidth = static_cast<int>(child->getAttribute("fitParentsWidth"));
          double relativeHeight = static_cast<double>(child->getAttribute("relativeHeight"));
          double relativeWidth = static_cast<double>(child->getAttribute("relativeWidth"));
          double aspectRatio = static_cast<double>(child->getAttribute("aspectRatio"));
          int rowStart = static_cast<int>(child->getAttribute("rowStart"));
          int rowStop = static_cast<int>(child->getAttribute("rowStop"));
          int colStart = static_cast<int>(child->getAttribute("colStart"));
          int colStop = static_cast<int>(child->getAttribute("colStop"));
          auto *slice = new grm::Slice(rowStart, rowStop, colStart, colStop);

          if (child->localName() == "layout_gridelement")
            {
              auto *curGridElement =
                  new grm::GridElement(absHeight, absWidth, absHeightPxl, absWidthPxl, fitParentsHeight,
                                       fitParentsWidth, relativeHeight, relativeWidth, aspectRatio);
              curGridElement->elementInDOM = child;
              grid->setElement(slice, curGridElement);
            }

          if (child->localName() == "layout_grid")
            {
              int nrows = static_cast<int>(child->getAttribute("nrows"));
              int ncols = static_cast<int>(child->getAttribute("ncols"));

              auto *curGrid =
                  new grm::Grid(nrows, ncols, absHeight, absWidth, absHeightPxl, absWidthPxl, fitParentsHeight,
                                fitParentsWidth, relativeHeight, relativeWidth, aspectRatio);
              curGrid->elementInDOM = child;
              grid->setElement(slice, curGrid);
              initializeGridElements(child, curGrid);
            }
        }
    }
}

static void finalizeGrid(const std::shared_ptr<GRM::Element> &root)
{
  grm::Grid *rootGrid = nullptr;
  if (root->hasChildNodes())
    {
      for (const auto &child : root->children())
        {
          if (child->localName() == "layout_grid")
            {
              int nrows = static_cast<int>(child->getAttribute("nrows"));
              int ncols = static_cast<int>(child->getAttribute("ncols"));
              rootGrid = new grm::Grid(nrows, ncols);
              child->setAttribute("plot_xmin", 0);
              child->setAttribute("plot_xmax", 1);
              child->setAttribute("plot_ymin", 0);
              child->setAttribute("plot_ymax", 1);

              initializeGridElements(child, rootGrid);
              rootGrid->finalizeSubplot();
              break;
            }
        }
    }
}

static void applyRootDefaults(std::shared_ptr<GRM::Element> root)
{
  if (!root->hasAttribute("clearws")) root->setAttribute("clearws", PLOT_DEFAULT_CLEAR);
  if (!root->hasAttribute("updatews")) root->setAttribute("updatews", PLOT_DEFAULT_UPDATE);
  if (!root->hasAttribute("_modified")) root->setAttribute("_modified", false);

  for (const auto &figure : root->children())
    {
      if (figure->localName() == "figure")
        {
          if (!figure->hasAttribute("size_x"))
            {
              figure->setAttribute("size_x", PLOT_DEFAULT_WIDTH);
              figure->setAttribute("size_x_type", "double");
              figure->setAttribute("size_x_unit", "px");
            }
          if (!figure->hasAttribute("size_y"))
            {
              figure->setAttribute("size_y", PLOT_DEFAULT_HEIGHT);
              figure->setAttribute("size_y_type", "double");
              figure->setAttribute("size_y_unit", "px");
            }

          for (const auto &child : figure->children())
            {
              if (child->localName() == "plot")
                {
                  if (!child->hasAttribute("kind")) child->setAttribute("kind", PLOT_DEFAULT_KIND);
                  if (!child->hasAttribute("keep_aspect_ratio"))
                    child->setAttribute("keep_aspect_ratio", PLOT_DEFAULT_KEEP_ASPECT_RATIO);
                  if (!child->hasAttribute("keep_window")) child->setAttribute("keep_window", PLOT_DEFAULT_KEEP_WINDOW);
                  if (!child->hasAttribute("plot_xmin")) child->setAttribute("plot_xmin", PLOT_DEFAULT_SUBPLOT_MIN_X);
                  if (!child->hasAttribute("plot_xmax")) child->setAttribute("plot_xmax", PLOT_DEFAULT_SUBPLOT_MAX_X);
                  if (!child->hasAttribute("plot_ymin")) child->setAttribute("plot_ymin", PLOT_DEFAULT_SUBPLOT_MIN_Y);
                  if (!child->hasAttribute("plot_ymax")) child->setAttribute("plot_ymax", PLOT_DEFAULT_SUBPLOT_MAX_Y);
                  auto kind = static_cast<std::string>(child->getAttribute("kind"));
                  if (!child->hasAttribute("adjust_xlim"))
                    {
                      if (kind == "heatmap" || kind == "marginalheatmap")
                        {
                          child->setAttribute("adjust_xlim", 0);
                        }
                      else
                        {
                          child->setAttribute("adjust_xlim",
                                              (child->hasAttribute("xlim_min") ? 0 : PLOT_DEFAULT_ADJUST_XLIM));
                        }
                    }
                  if (!child->hasAttribute("adjust_ylim"))
                    {
                      if (kind == "heatmap" || kind == "marginalheatmap")
                        {
                          child->setAttribute("adjust_ylim", 0);
                        }
                      else
                        {
                          child->setAttribute("adjust_ylim",
                                              (child->hasAttribute("ylim_min") ? 0 : PLOT_DEFAULT_ADJUST_YLIM));
                        }
                    }
                  if (!child->hasAttribute("adjust_zlim"))
                    {
                      if (kind != "heatmap" && kind != "marginalheatmap")
                        {
                          child->setAttribute("adjust_zlim",
                                              (child->hasAttribute("zlim_min") ? 0 : PLOT_DEFAULT_ADJUST_ZLIM));
                        }
                    }
                  if (!child->hasAttribute("linespec")) child->setAttribute("linespec", " ");
                  if (!child->hasAttribute("xlog")) child->setAttribute("xlog", PLOT_DEFAULT_XLOG);
                  if (!child->hasAttribute("ylog")) child->setAttribute("ylog", PLOT_DEFAULT_YLOG);
                  if (!child->hasAttribute("zlog")) child->setAttribute("zlog", PLOT_DEFAULT_ZLOG);
                  if (!child->hasAttribute("xflip")) child->setAttribute("xflip", PLOT_DEFAULT_XFLIP);
                  if (!child->hasAttribute("yflip")) child->setAttribute("yflip", PLOT_DEFAULT_YFLIP);
                  if (!child->hasAttribute("zflip")) child->setAttribute("zflip", PLOT_DEFAULT_ZFLIP);
                  if (!child->hasAttribute("resample_method"))
                    child->setAttribute("resample_method", (int)PLOT_DEFAULT_RESAMPLE_METHOD);
                  if (!child->hasAttribute("font")) child->setAttribute("font", PLOT_DEFAULT_FONT);
                  if (!child->hasAttribute("font_precision"))
                    child->setAttribute("font_precision", PLOT_DEFAULT_FONT_PRECISION);
                  if (!child->hasAttribute("colormap")) child->setAttribute("colormap", PLOT_DEFAULT_COLORMAP);
                }
            }
        }
    }
}

void GRM::Render::render(const std::shared_ptr<GRM::Document> &document,
                         const std::shared_ptr<GRM::Context> &extContext)
{
  /*!
   * static GRM::Render::render receiving external document and context
   *
   * \param[in] document A GRM::Document that will be rendered
   * \param[in] extContext A GRM::Context
   */
  auto root = document->firstChildElement();
  global_root->setAttribute("_modified", false);
  if (root->hasChildNodes())
    {
      for (const auto &child : root->children())
        {
          gr_savestate();
          ::renderHelper(child, extContext);
          gr_restorestate();
        }
    }
  global_root->setAttribute("_modified", false); // reset the modified flag, cause all updates are made
}

void GRM::Render::render(std::shared_ptr<GRM::Document> const &document)
{
  /*!
   * GRM::Render::render that receives an external document but uses the GRM::Render instance's context.
   *
   * \param[in] document A GRM::Document that will be rendered
   */
  auto root = document->firstChildElement();
  global_root->setAttribute("_modified", false);
  if (root->hasChildNodes())
    {
      for (const auto &child : root->children())
        {
          gr_savestate();
          ::renderHelper(child, this->context);
          gr_restorestate();
        }
    }
  global_root->setAttribute("_modified", false); // reset the modified flag, cause all updates are made
}

void GRM::Render::render(const std::shared_ptr<GRM::Context> &extContext)
{
  /*!
   *GRM::Render::render uses GRM::Render instance's document and an external context
   *
   * \param[in] extContext A GRM::Context
   */
  auto root = this->firstChildElement();
  global_root->setAttribute("_modified", false);
  if (root->hasChildNodes())
    {
      for (const auto &child : root->children())
        {
          gr_savestate();
          ::renderHelper(child, extContext);
          gr_restorestate();
        }
    }
  global_root->setAttribute("_modified", false); // reset the modified flag, cause all updates are made
}

void GRM::Render::render()
{
  /*!
   * GRM::Render::render uses both instance's document and context
   */
  auto root = this->firstChildElement();
  global_root = root;
  if (root->hasChildNodes())
    {
      active_figure = this->firstChildElement()->querySelectorsAll("[active=1]")[0];
      const unsigned int indent = 2;

      redrawws = true;
      bounding_id = 0;
      if (!global_render) GRM::Render::createRender();
      applyRootDefaults(root);
      if (logger_enabled())
        {
          std::cerr << toXML(root, GRM::SerializerOptions{std::string(indent, ' '), true}) << "\n";
        }
      if (static_cast<int>(root->getAttribute("clearws"))) gr_clearws();
      root->setAttribute("_modified", true);
      renderHelper(root, this->context);
      renderZQueue(this->context);
      root->setAttribute("_modified", false); // reset the modified flag, cause all updates are made
      if (getenv("GRPLOT_ENABLE_EDITOR")) missing_bbox_calculator(root, this->context);
      if (root->hasAttribute("updatews") && static_cast<int>(root->getAttribute("updatews"))) gr_updatews();
      // needed when series_line is changed to series_scatter for example
      if (getenv("GRPLOT_ENABLE_EDITOR"))
        {
          for (const auto &child : global_render->querySelectorsAll("[_bbox_id=-1]"))
            {
              child->removeAttribute("_bbox_id");
              missing_bbox_calculator(child, this->context);
            }
        }
      if (logger_enabled())
        {
          std::cerr << toXML(root, GRM::SerializerOptions{std::string(indent, ' '), true}) << "\n";
        }
      redrawws = false;
      // reset markertypes
      previous_scatter_marker_type = plot_scatter_markertypes;
      previous_line_marker_type = plot_scatter_markertypes;
    }
}

void GRM::Render::process_tree()
{
  global_root->setAttribute("_modified", true);
  renderHelper(global_root, this->context);
  renderZQueue(this->context);
  global_root->setAttribute("_modified", false); // reset the modified flag, cause all updates are made
}

void GRM::Render::finalize()
{
  grContextIDManager.destroyGRContexts();
}

std::shared_ptr<GRM::Render> GRM::Render::createRender()
{
  /*!
   * This function can be used to create a Render object
   */
  global_render = std::shared_ptr<Render>(new Render());
  global_render->ownerDocument()->setUpdateFct(&renderCaller, &updateFilter);
  global_render->ownerDocument()->setContextFct(&deleteContextAttribute, &updateContextAttribute);
  return global_render;
}

GRM::Render::Render()
{
  /*!
   * This is the constructor for GRM::Render
   */
  this->context = std::shared_ptr<GRM::Context>(new Context());
}

std::shared_ptr<GRM::Context> GRM::Render::getContext()
{
  return context;
}

/*
 * Searches in elementToTooltip for attributeName and returns a string vector
 * containing:
 * [0] The default value for this attribute
 * [1] The description for this attribute
 */
std::vector<std::string> GRM::Render::getDefaultAndTooltip(const std::shared_ptr<Element> &element,
                                                           std::string attributeName)
{
  static std::unordered_map<std::string, std::map<std::string, std::vector<std::string>>> elementToTooltip{
      {std::string("polyline"),
       std::map<std::string, std::vector<std::string>>{
           {std::string("linecolorind"),
            std::vector<std::string>{"989", "Sets the linecolor according to the current colormap"}},
           {std::string("x"), std::vector<std::string>{"None", "References the x-values stored in the context"}},
           {std::string("y"), std::vector<std::string>{"None", "References the y-values stored in the context"}},
           {std::string("x1"), std::vector<std::string>{"None", "The beginning x-coordinate"}},
           {std::string("y1"), std::vector<std::string>{"None", "The beginning y-coordinate"}},
           {std::string("x2"), std::vector<std::string>{"None", "The ending x-coordinate"}},
           {std::string("y2"), std::vector<std::string>{"None", "The ending y-coordinate"}},
       }},
      {std::string("text"),
       std::map<std::string, std::vector<std::string>>{
           {std::string("x"), std::vector<std::string>{"None", "X-position of the text"}},
           {std::string("y"), std::vector<std::string>{"None", "Y-position of the text"}},
           {std::string("text"), std::vector<std::string>{"Title", "The text diplayed by this node"}},
           {std::string("render_method"),
            std::vector<std::string>{"gr_text", "Render method used to display the text"}},
           {std::string("textalign_horizontal"), std::vector<std::string>{"2", "Use horizontal alignment method"}},
           {std::string("textalign_vertical"), std::vector<std::string>{"1", "Use vertical alignment method"}},
       }},
      {std::string("grid"),
       std::map<std::string, std::vector<std::string>>{
           {std::string("x_tick"),
            std::vector<std::string>{"1", "The interval between minor tick marks on the X-axis"}},
           {std::string("y_tick"),
            std::vector<std::string>{"1", "The interval between minor tick marks on the Y-axis"}},
           {std::string("x_org"),
            std::vector<std::string>{"0", "The world coordinates of the origin (point of intersection) of the X-axis"}},
           {std::string("y_org"),
            std::vector<std::string>{"0", "The world coordinates of the origin (point of intersection) of the Y-axis"}},
           {std::string("major_x"),
            std::vector<std::string>{"5", "Unitless integer values specifying the number of minor tick intervals "
                                          "between major tick marks. Values of 0 or 1 imply no minor ticks. Negative "
                                          "values specify no labels will be drawn for the X-axis"}},
           {std::string("major_y"),
            std::vector<std::string>{"5", "Unitless integer values specifying the number of minor tick intervals "
                                          "between major tick marks. Values of 0 or 1 imply no minor ticks. Negative "
                                          "values specify no labels will be drawn for the Y-axis"}},
       }}};
  if (elementToTooltip.count(element->localName()) &&
      elementToTooltip[element->localName().c_str()].count(attributeName))
    {
      return elementToTooltip[element->localName().c_str()][attributeName];
    }
  else
    {
      return std::vector<std::string>{"", "No description found"};
    }
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~ create functions ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 */

std::shared_ptr<GRM::Element>
GRM::Render::createPolymarker(const std::string &x_key, std::optional<std::vector<double>> x, const std::string &y_key,
                              std::optional<std::vector<double>> y, const std::shared_ptr<GRM::Context> &extContext,
                              int marker_type, double marker_size, int marker_colorind,
                              const std::shared_ptr<GRM::Element> &extElement)
{
  /*!
   * This function can be used to create a Polymarker GRM::Element
   *
   * \param[in] x_key A string used for storing the x coordinates in GRM::Context
   * \param[in] x A vector containing double values representing x coordinates
   * \param[in] y_key A string used for storing the y coordinates in GRM::Context
   * \param[in] y A vector containing double values representing y coordinates
   * \param[in] extContext A GRM::Context that is used for storing the vectors. By default it uses GRM::Render's
   * GRM::Context object but an external GRM::Context can be used \param[in] marker_type An Integer setting the
   * gr_markertype. By default it is 0 \param[in] marker_size A Double value setting the gr_markersize. By default it
   * is 0.0 \param[in] marker_colorind An Integer setting the gr_markercolorind. By default it is 0
   */

  std::shared_ptr<GRM::Context> useContext = (extContext == nullptr) ? context : extContext;
  std::shared_ptr<GRM::Element> element = (extElement == nullptr) ? createElement("polymarker") : extElement;
  if (x != std::nullopt)
    {
      (*useContext)[x_key] = x.value();
    }
  element->setAttribute("x", x_key);

  if (y != std::nullopt)
    {
      (*useContext)[y_key] = y.value();
    }
  element->setAttribute("y", y_key);

  if (marker_type != 0)
    {
      element->setAttribute("markertype", marker_type);
    }
  if (marker_size != 0.0)
    {
      element->setAttribute("markersize", marker_size);
    }
  if (marker_colorind != 0)
    {
      element->setAttribute("markercolorind", marker_colorind);
    }
  return element;
}

std::shared_ptr<GRM::Element> GRM::Render::createPolymarker(double x, double y, int marker_type, double marker_size,
                                                            int marker_colorind,
                                                            const std::shared_ptr<GRM::Element> &extElement)
{
  std::shared_ptr<GRM::Element> element = (extElement == nullptr) ? createElement("polymarker") : extElement;
  element->setAttribute("x", x);
  element->setAttribute("y", y);
  if (marker_type != 0)
    {
      element->setAttribute("markertype", marker_type);
    }
  if (marker_size != 0.0)
    {
      element->setAttribute("markersize", marker_size);
    }
  if (marker_colorind != 0)
    {
      element->setAttribute("markercolorind", marker_colorind);
    }
  return element;
}

std::shared_ptr<GRM::Element> GRM::Render::createPolyline(double x1, double x2, double y1, double y2, int line_type,
                                                          double line_width, int line_colorind,
                                                          const std::shared_ptr<GRM::Element> &extElement)
{
  std::shared_ptr<GRM::Element> element = (extElement == nullptr) ? createElement("polyline") : extElement;
  element->setAttribute("x1", x1);
  element->setAttribute("x2", x2);
  element->setAttribute("y1", y1);
  element->setAttribute("y2", y2);
  if (line_type != 0)
    {
      element->setAttribute("linetype", line_type);
    }
  if (line_width != 0.0)
    {
      element->setAttribute("linewidth", line_width);
    }
  if (line_colorind != 0)
    {
      element->setAttribute("linecolorind", line_colorind);
    }
  return element;
}

std::shared_ptr<GRM::Element>
GRM::Render::createPolyline(const std::string &x_key, std::optional<std::vector<double>> x, const std::string &y_key,
                            std::optional<std::vector<double>> y, const std::shared_ptr<GRM::Context> &extContext,
                            int line_type, double line_width, int line_colorind,
                            const std::shared_ptr<GRM::Element> &extElement)
{
  /*!
   * This function can be used to create a Polyline GRM::Element
   *
   * \param[in] x_key A string used for storing the x coordinates in GRM::Context
   * \param[in] x A vector containing double values representing x coordinates
   * \param[in] y_key A string used for storing the y coordinates in GRM::Context
   * \param[in] y A vector containing double values representing y coordinates
   * \param[in] extContext A GRM::Context that is used for storing the vectors. By default it uses GRM::Render's
   * GRM::Context object but an external GRM::Context can be used \param[in] line_type An Integer setting the
   * gr_linertype. By default it is 0 \param[in] line_width A Double value setting the gr_linewidth. By default it is
   * 0.0 \param[in] marker_colorind An Integer setting the gr_linecolorind. By default it is 0
   */

  std::shared_ptr<GRM::Context> useContext = (extContext == nullptr) ? context : extContext;
  std::shared_ptr<GRM::Element> element = (extElement == nullptr) ? createElement("polyline") : extElement;
  if (x != std::nullopt)
    {
      (*useContext)[x_key] = *x;
    }
  element->setAttribute("x", x_key);
  if (y != std::nullopt)
    {
      (*useContext)[y_key] = *y;
    }
  element->setAttribute("y", y_key);
  if (line_type != 0)
    {
      element->setAttribute("linetype", line_type);
    }
  if (line_width != 0.0)
    {
      element->setAttribute("linewidth", line_width);
    }
  if (line_colorind != 0)
    {
      element->setAttribute("linecolorind", line_colorind);
    }
  return element;
}

std::shared_ptr<GRM::Element> GRM::Render::createText(double x, double y, const std::string &text,
                                                      CoordinateSpace space,
                                                      const std::shared_ptr<GRM::Element> &extElement)
{
  /*!
   * This function can be used to create a Text GRM::Element
   *
   * \param[in] x A double value representing the x coordinate
   * \param[in] y A double value representing the y coordinate
   * \param[in] text A string
   * \param[in] space the coordinate space (WC or NDC) for x and y, default NDC
   */

  std::shared_ptr<GRM::Element> element = (extElement == nullptr) ? createElement("text") : extElement;
  element->setAttribute("x", x);
  element->setAttribute("y", y);
  element->setAttribute("text", text);
  element->setAttribute("space", static_cast<int>(space));
  return element;
}

std::shared_ptr<GRM::Element>
GRM::Render::createFillArea(const std::string &x_key, std::optional<std::vector<double>> x, const std::string &y_key,
                            std::optional<std::vector<double>> y, const std::shared_ptr<GRM::Context> &extContext,
                            int fillintstyle, int fillstyle, int fillcolorind,
                            const std::shared_ptr<GRM::Element> &extElement)
{
  /*!
   * This function can be used to create a FillArea GRM::Element
   *
   * \param[in] n The number of data points
   * \param[in] x_key A string used for storing the x coordinates in GRM::Context
   * \param[in] x A vector containing double values representing x coordinates
   * \param[in] y_key A string used for storing the y coordinates in GRM::Context
   * \param[in] y A vector containing double values representing y coordinates
   * \param[in] extContext A GRM::Context that is used for storing the vectors. By default it uses GRM::Render's
   * GRM::Context object but an external GRM::Context can be used \param[in] fillintstyle An Integer setting the
   * gr_fillintstyle. By default it is 0 \param[in] fillstyle An Integer setting the gr_fillstyle. By default it is 0
   */

  std::shared_ptr<GRM::Context> useContext = (extContext == nullptr) ? context : extContext;
  std::shared_ptr<GRM::Element> element = (extElement == nullptr) ? createElement("fillarea") : extElement;
  if (x != std::nullopt)
    {
      (*useContext)[x_key] = *x;
    }
  element->setAttribute("x", x_key);
  if (y != std::nullopt)
    {
      (*useContext)[y_key] = *y;
    }
  element->setAttribute("y", y_key);

  if (fillintstyle != 0)
    {
      element->setAttribute("fillintstyle", fillintstyle);
    }
  if (fillstyle != 0)
    {
      element->setAttribute("fillstyle", fillstyle);
    }
  if (fillcolorind != -1)
    {
      element->setAttribute("fillcolorind", fillcolorind);
    }
  return element;
}

std::shared_ptr<GRM::Element> GRM::Render::createCellArray(double xmin, double xmax, double ymin, double ymax, int dimx,
                                                           int dimy, int scol, int srow, int ncol, int nrow,
                                                           const std::string &color_key,
                                                           std::optional<std::vector<int>> color,
                                                           const std::shared_ptr<GRM::Context> &extContext,
                                                           const std::shared_ptr<GRM::Element> &extElement)
{
  /*!
   * This function can be used to create a CellArray GRM::Element
   *
   * \param[in] xmin A double value
   * \param[in] xmax A double value
   * \param[in] ymin A double value
   * \param[in] ymax A double value
   * \param[in] dimx An Integer value
   * \param[in] dimy An Integer value
   * \param[in] scol An Integer value
   * \param[in] srow An Integer value
   * \param[in] ncol An Integer value
   * \param[in] nrow An Integer value
   * \param[in] color_key A string used as a key for storing color
   * \param[in] color A vector with Integers
   * \param[in] extContext A GRM::Context used for storing color. By default it uses GRM::Render's GRM::Context object
   * but an external GRM::Context can be used
   */

  std::shared_ptr<GRM::Context> useContext = (extContext == nullptr) ? context : extContext;
  std::shared_ptr<GRM::Element> element = (extElement == nullptr) ? createElement("cellarray") : extElement;
  element->setAttribute("xmin", xmin);
  element->setAttribute("xmax", xmax);
  element->setAttribute("ymin", ymin);
  element->setAttribute("ymax", ymax);
  element->setAttribute("dimx", dimx);
  element->setAttribute("dimy", dimy);
  element->setAttribute("scol", scol);
  element->setAttribute("srow", srow);
  element->setAttribute("ncol", ncol);
  element->setAttribute("nrow", nrow);
  element->setAttribute("color", color_key);
  if (color != std::nullopt)
    {
      (*useContext)[color_key] = *color;
    }
  return element;
}

std::shared_ptr<GRM::Element> GRM::Render::createAxes(double x_tick, double y_tick, double x_org, double y_org,
                                                      int x_major, int y_major, int tick_orientation,
                                                      const std::shared_ptr<GRM::Element> &extElement)
{
  /*!
   * This function can be used for creating an Axes GRM::Element
   *
   * \param[in] x_tick A double value
   * \param[in] y_tick A double value
   * \param[in] x_org A double value
   * \param[in] y_org A double value
   * \param[in] x_major An Integer value
   * \param[in] y_major An Integer value
   * \param[in] tick_orientation A Double value
   */
  std::shared_ptr<GRM::Element> element = (extElement == nullptr) ? createElement("axes") : extElement;
  element->setAttribute("x_tick", x_tick);
  element->setAttribute("y_tick", y_tick);
  element->setAttribute("x_org", x_org);
  element->setAttribute("y_org", y_org);
  element->setAttribute("x_major", x_major);
  element->setAttribute("y_major", y_major);
  element->setAttribute("tick_orientation", tick_orientation);
  return element;
}

std::shared_ptr<GRM::Element> GRM::Render::createEmptyAxes(int tick_orientation)
{
  /*!
   * This function can be used for creating an Axes GRM::Element with missing information
   *
   * \param[in] tick_orientation A Int value specifing the direction of the ticks
   */
  auto element = createElement("axes");
  element->setAttribute("tick_orientation", tick_orientation);
  return element;
}

std::shared_ptr<GRM::Element> GRM::Render::createLegend(const std::string &labels_key,
                                                        std::optional<std::vector<std::string>> labels,
                                                        const std::string &specs_key,
                                                        std::optional<std::vector<std::string>> specs,
                                                        const std::shared_ptr<GRM::Context> &extContext,
                                                        const std::shared_ptr<GRM::Element> &extElement)
{
  /*!
   * This function can be used for creating a legend GRM::Element
   * This element is different compared to most of Render's GRM::Element, the legend GRM::Element will incorporate
   * plot_draw_legend code from plot.cxx and will create new GRM::Elements as child nodes in the render document
   *
   * \param[in] labels_key A std::string for the labels vector
   * \param[in] labels May be an std::vector<std::string>> containing the labels
   * \param[in] spec An std::string
   */

  std::shared_ptr<GRM::Element> element = (extElement == nullptr) ? createElement("legend") : extElement;
  element->setAttribute("z_index", 4);
  std::shared_ptr<GRM::Context> useContext = (extContext == nullptr) ? context : extContext;
  element->setAttribute("specs", specs_key);
  element->setAttribute("labels", labels_key);

  if (labels != std::nullopt)
    {
      (*useContext)[labels_key] = *labels;
    }
  if (specs != std::nullopt)
    {
      (*useContext)[specs_key] = *specs;
    }

  return element;
}

std::shared_ptr<GRM::Element> GRM::Render::createDrawPolarAxes(int angle_ticks, const std::string &kind, int phiflip,
                                                               const std::string &norm, double tick, double line_width,
                                                               const std::shared_ptr<GRM::Element> &extElement)
{
  std::shared_ptr<GRM::Element> element = (extElement == nullptr) ? createElement("polar_axes") : extElement;
  if (!norm.empty())
    {
      element->setAttribute("norm", norm);
    }
  if (tick != 0.0)
    {
      element->setAttribute("tick", tick);
    }
  if (line_width != 0.0)
    {
      element->setAttribute("linewidth", line_width);
    }
  element->setAttribute("angle_ticks", angle_ticks);
  // todo should phiflip be passed when creating a polarAxesElement
  //  element->setAttribute("phiflip", phiflip);

  return element;
}

std::shared_ptr<GRM::Element> GRM::Render::createPieLegend(const std::string &labels_key,
                                                           std::optional<std::vector<std::string>> labels,
                                                           const std::shared_ptr<GRM::Context> &extContext,
                                                           const std::shared_ptr<GRM::Element> &extElement)
{
  std::shared_ptr<GRM::Element> element = (extElement == nullptr) ? createElement("legend") : extElement;
  std::shared_ptr<GRM::Context> useContext = (extContext == nullptr) ? context : extContext;
  element->setAttribute("labels", labels_key);

  if (labels != std::nullopt)
    {
      (*useContext)[labels_key] = *labels;
    }
  return element;
}

std::shared_ptr<GRM::Element> GRM::Render::createPieSegment(const double start_angle, const double end_angle,
                                                            const std::string text, const int color_index,
                                                            const std::shared_ptr<GRM::Element> &extElement)
{
  std::shared_ptr<GRM::Element> element = (extElement == nullptr) ? createElement("pie_segment") : extElement;
  element->setAttribute("start_angle", start_angle);
  element->setAttribute("end_angle", end_angle);
  element->setAttribute("text", text);
  element->setAttribute("color_ind", color_index);

  return element;
}

std::shared_ptr<GRM::Element> GRM::Render::createBar(const double x1, const double x2, const double y1, const double y2,
                                                     const int bar_color_index, const int edge_color_index,
                                                     const std::string bar_color_rgb, const std::string edge_color_rgb,
                                                     const double linewidth, const std::string text,
                                                     const std::shared_ptr<GRM::Element> &extElement)
{
  std::shared_ptr<GRM::Element> element = (extElement == nullptr) ? createElement("bar") : extElement;
  element->setAttribute("x1", x1);
  element->setAttribute("x2", x2);
  element->setAttribute("y1", y1);
  element->setAttribute("y2", y2);
  element->setAttribute("edge_color_index", edge_color_index);
  element->setAttribute("bar_color_index", bar_color_index);
  if (!bar_color_rgb.empty()) element->setAttribute("bar_color_rgb", bar_color_rgb);
  if (!edge_color_rgb.empty()) element->setAttribute("edge_color_rgb", edge_color_rgb);
  if (linewidth != -1) element->setAttribute("linewidth", linewidth);
  if (!text.empty()) element->setAttribute("text", text);

  return element;
}

std::shared_ptr<GRM::Element> GRM::Render::createGrid(double x_tick, double y_tick, double x_org, double y_org,
                                                      int major_x, int major_y,
                                                      const std::shared_ptr<GRM::Element> &extElement)
{
  std::shared_ptr<GRM::Element> element = (extElement == nullptr) ? createElement("grid") : extElement;
  element->setAttribute("x_tick", x_tick);
  element->setAttribute("y_tick", y_tick);
  element->setAttribute("x_org", x_org);
  element->setAttribute("y_org", y_org);
  element->setAttribute("major_x", major_x);
  element->setAttribute("major_y", major_y);
  return element;
}

std::shared_ptr<GRM::Element> GRM::Render::createEmptyGrid(bool x_grid, bool y_grid)
{
  auto element = createElement("grid");
  if (!x_grid)
    {
      element->setAttribute("x_tick", 0);
    }
  if (!y_grid)
    {
      element->setAttribute("y_tick", 0);
    }
  return element;
}

std::shared_ptr<GRM::Element> GRM::Render::createSeries(const std::string &name)
{
  auto element = createElement("series_" + name);
  element->setAttribute("kind", name);
  element->setAttribute("_update_required", false);
  element->setAttribute("_delete_children", 0);
  return element;
}

std::shared_ptr<GRM::Element> GRM::Render::createDrawImage(double xmin, double ymin, double xmax, double ymax,
                                                           int width, int height, const std::string &data_key,
                                                           std::optional<std::vector<int>> data, int model,
                                                           const std::shared_ptr<GRM::Context> &extContext,
                                                           const std::shared_ptr<GRM::Element> &extElement)
{
  /*!
   * This function can be used to create a DrawImage GRM::Element
   *
   * \param[in] xmin A Double value
   * \param[in] xmax A Double value
   * \param[in] ymin A Double value
   * \param[in] ymax A Double value
   * \param[in] width An Integer value
   * \param[in] height An Integer value
   * \param[in] data_key A String used as a key for storing data
   * \param[in] data A vector containing Integers
   * \param[in] model An Integer setting the model
   * \param[in] extContext A GRM::Context used for storing data. By default it uses GRM::Render's GRM::Context object
   * but an external GRM::Context can be used
   */
  std::shared_ptr<GRM::Context> useContext = (extContext == nullptr) ? context : extContext;
  std::shared_ptr<GRM::Element> element = (extElement == nullptr) ? createElement("drawimage") : extElement;
  element->setAttribute("xmin", xmin);
  element->setAttribute("xmax", xmax);
  element->setAttribute("ymin", ymin);
  element->setAttribute("ymax", ymax);
  element->setAttribute("width", width);
  element->setAttribute("height", height);
  element->setAttribute("model", model);
  element->setAttribute("data", data_key);
  if (data != std::nullopt)
    {
      (*useContext)[data_key] = *data;
    }
  return element;
}

std::shared_ptr<GRM::Element> GRM::Render::createDrawArc(double xmin, double xmax, double ymin, double ymax, double a1,
                                                         double a2, const std::shared_ptr<GRM::Element> &extElement)
{
  std::shared_ptr<GRM::Element> element = (extElement == nullptr) ? createElement("drawarc") : extElement;
  element->setAttribute("xmin", xmin);
  element->setAttribute("xmax", xmax);
  element->setAttribute("ymin", ymin);
  element->setAttribute("ymax", ymax);
  element->setAttribute("a1", a1);
  element->setAttribute("a2", a2);

  return element;
}

std::shared_ptr<GRM::Element> GRM::Render::createFillArc(double xmin, double xmax, double ymin, double ymax, double a1,
                                                         double a2, int fillintstyle, int fillstyle, int fillcolorind,
                                                         const std::shared_ptr<GRM::Element> &extElement)
{
  std::shared_ptr<GRM::Element> element = (extElement == nullptr) ? createElement("fillarc") : extElement;
  element->setAttribute("xmin", xmin);
  element->setAttribute("xmax", xmax);
  element->setAttribute("ymin", ymin);
  element->setAttribute("ymax", ymax);
  element->setAttribute("a1", a1);
  element->setAttribute("a2", a2);

  if (fillintstyle != 0)
    {
      element->setAttribute("fillintstyle", fillintstyle);
    }
  if (fillstyle != 0)
    {
      element->setAttribute("fillstyle", fillstyle);
    }
  if (fillcolorind != -1)
    {
      element->setAttribute("fillcolorind", fillcolorind);
    }
  return element;
}

std::shared_ptr<GRM::Element> GRM::Render::createDrawRect(double xmin, double xmax, double ymin, double ymax,
                                                          const std::shared_ptr<GRM::Element> &extElement)
{
  std::shared_ptr<GRM::Element> element = (extElement == nullptr) ? createElement("drawrect") : extElement;
  element->setAttribute("xmin", xmin);
  element->setAttribute("xmax", xmax);
  element->setAttribute("ymin", ymin);
  element->setAttribute("ymax", ymax);

  return element;
}

std::shared_ptr<GRM::Element> GRM::Render::createFillRect(double xmin, double xmax, double ymin, double ymax,
                                                          int fillintstyle, int fillstyle, int fillcolorind,
                                                          const std::shared_ptr<GRM::Element> &extElement)
{
  std::shared_ptr<GRM::Element> element = (extElement == nullptr) ? createElement("fillrect") : extElement;
  element->setAttribute("xmin", xmin);
  element->setAttribute("xmax", xmax);
  element->setAttribute("ymin", ymin);
  element->setAttribute("ymax", ymax);

  if (fillintstyle != 0)
    {
      element->setAttribute("fillintstyle", fillintstyle);
    }
  if (fillstyle != 0)
    {
      element->setAttribute("fillstyle", fillstyle);
    }
  if (fillcolorind != -1)
    {
      element->setAttribute("fillcolorind", fillcolorind);
    }

  return element;
}

std::shared_ptr<GRM::Element> GRM::Render::createQuiver(const std::string &x_key, std::optional<std::vector<double>> x,
                                                        const std::string &y_key, std::optional<std::vector<double>> y,
                                                        const std::string &u_key, std::optional<std::vector<double>> u,
                                                        const std::string &v_key, std::optional<std::vector<double>> v,
                                                        int color, const std::shared_ptr<GRM::Context> &extContext)
{
  /*
   * This function can be used to create a Quiver GRM::Element
   *
   */
  std::shared_ptr<GRM::Context> useContext = (extContext == nullptr) ? context : extContext;
  auto element = createSeries("quiver");
  element->setAttribute("x", x_key);
  element->setAttribute("y", y_key);
  element->setAttribute("u", u_key);
  element->setAttribute("v", v_key);
  element->setAttribute("color", color);

  if (x != std::nullopt)
    {
      (*useContext)[x_key] = *x;
    }
  if (y != std::nullopt)
    {
      (*useContext)[y_key] = *y;
    }
  if (u != std::nullopt)
    {
      (*useContext)[u_key] = *u;
    }
  if (v != std::nullopt)
    {
      (*useContext)[v_key] = *v;
    }

  return element;
}

std::shared_ptr<GRM::Element> GRM::Render::createHexbin(const std::string &x_key, std::optional<std::vector<double>> x,
                                                        const std::string &y_key, std::optional<std::vector<double>> y,
                                                        const std::shared_ptr<GRM::Context> &extContext)
{
  /*!
   * This function can be used to create a hexbin GRM::Element
   */
  std::shared_ptr<GRM::Context> useContext = (extContext == nullptr) ? context : extContext;
  auto element = createSeries("hexbin");
  element->setAttribute("x", x_key);
  element->setAttribute("y", y_key);

  if (x != std::nullopt)
    {
      (*useContext)[x_key] = *x;
    }
  if (y != std::nullopt)
    {
      (*useContext)[y_key] = *y;
    }

  return element;
}

std::shared_ptr<GRM::Element> GRM::Render::createColorbar(unsigned int colors,
                                                          const std::shared_ptr<GRM::Context> &extContext,
                                                          const std::shared_ptr<GRM::Element> &extElement)
{
  /*!
   * This function can be used to create a colorbar GRM::Element
   */
  std::shared_ptr<GRM::Context> useContext = (extContext == nullptr) ? context : extContext;
  std::shared_ptr<GRM::Element> element = (extElement == nullptr) ? createElement("colorbar") : extElement;
  element->setAttribute("colors", static_cast<int>(colors));
  element->setAttribute("_update_required", false);
  element->setAttribute("_delete_children", 0);

  return element;
}

std::shared_ptr<GRM::Element> GRM::Render::createPolarCellArray(
    double x_org, double y_org, double phimin, double phimax, double rmin, double rmax, int dimphi, int dimr, int scol,
    int srow, int ncol, int nrow, const std::string &color_key, std::optional<std::vector<int>> color,
    const std::shared_ptr<Context> &extContext, const std::shared_ptr<GRM::Element> &extElement)
{
  /*!
   * Display a two dimensional color index array mapped to a disk using polar
   * coordinates.
   *
   * \param[in] x_org X coordinate of the disk center in world coordinates
   * \param[in] y_org Y coordinate of the disk center in world coordinates
   * \param[in] phimin start angle of the disk sector in degrees
   * \param[in] phimax end angle of the disk sector in degrees
   * \param[in] rmin inner radius of the punctured disk in world coordinates
   * \param[in] rmax outer radius of the disk in world coordinates
   * \param[in] dimphi Phi (X) dimension of the color index array
   * \param[in] dimr R (Y) dimension of the color index array
   * \param[in] scol number of leading columns in the color index array
   * \param[in] srow number of leading rows in the color index array
   * \param[in] ncol total number of columns in the color index array
   * \param[in] nrow total number of rows in the color index array
   * \param[in] color color index array
   *
   * The two dimensional color index array is mapped to the resulting image by
   * interpreting the X-axis of the array as the angle and the Y-axis as the radius.
   * The center point of the resulting disk is located at `x_org`, `y_org` and the
   * radius of the disk is `rmax`.
   *
   * To draw a contiguous array as a complete disk use:
   *
   *     gr_polarcellarray(x_org, y_org, 0, 360, 0, rmax, dimphi, dimr, 1, 1, dimphi, dimr, color)
   *
   * The additional parameters to the function can be used to further control the
   * mapping from polar to cartesian coordinates.
   *
   * If `rmin` is greater than 0 the input data is mapped to a punctured disk (or
   * annulus) with an inner radius of `rmin` and an outer radius `rmax`. If `rmin`
   * is greater than `rmax` the Y-axis of the array is reversed.
   *
   * The parameter `phimin` and `phimax` can be used to map the data to a sector
   * of the (punctured) disk starting at `phimin` and ending at `phimax`. If
   * `phimin` is greater than `phimax` the X-axis is reversed. The visible sector
   * is the one starting in mathematically positive direction (counterclockwise)
   * at the smaller angle and ending at the larger angle. An example of the four
   * possible options can be found below:
   *
   * \verbatim embed:rst:leading-asterisk
   *
   * +-----------+-----------+---------------------------------------------------+
   * |**phimin** |**phimax** |**Result**                                         |
   * +-----------+-----------+---------------------------------------------------+
   * |90         |270        |Left half visible, mapped counterclockwise         |
   * +-----------+-----------+---------------------------------------------------+
   * |270        |90         |Left half visible, mapped clockwise                |
   * +-----------+-----------+---------------------------------------------------+
   * |-90        |90         |Right half visible, mapped counterclockwise        |
   * +-----------+-----------+---------------------------------------------------+
   * |90         |-90        |Right half visible, mapped clockwise               |
   * +-----------+-----------+---------------------------------------------------+
   *
   * \endverbatim
   *
   * `scol` and `srow` can be used to specify a (1-based) starting column and row
   * in the `color` array. `ncol` and `nrow` specify the actual dimension of the
   * array in the memory whereof `dimphi` and `dimr` values are mapped to the disk.
   *
   */

  std::shared_ptr<GRM::Context> useContext = (extContext == nullptr) ? context : extContext;
  std::shared_ptr<GRM::Element> element = (extElement == nullptr) ? createElement("polarcellarray") : extElement;
  element->setAttribute("x_org", x_org);
  element->setAttribute("y_org", y_org);
  element->setAttribute("phimin", phimin);
  element->setAttribute("phimax", phimax);
  element->setAttribute("rmin", rmin);
  element->setAttribute("rmax", rmax);
  element->setAttribute("dimphi", dimphi);
  element->setAttribute("dimr", dimr);
  element->setAttribute("scol", scol);
  element->setAttribute("srow", srow);
  element->setAttribute("ncol", ncol);
  element->setAttribute("nrow", nrow);
  element->setAttribute("color", color_key);
  if (color != std::nullopt)
    {
      (*useContext)[color_key] = *color;
    }
  return element;
}

std::shared_ptr<GRM::Element> GRM::Render::createNonUniformPolarCellArray(
    double x_org, double y_org, const std::string &phi_key, std::optional<std::vector<double>> phi,
    const std::string &r_key, std::optional<std::vector<double>> r, int dimphi, int dimr, int scol, int srow, int ncol,
    int nrow, const std::string &color_key, std::optional<std::vector<int>> color,
    const std::shared_ptr<GRM::Context> &extContext, const std::shared_ptr<GRM::Element> &extElement)
{
  /*!
   * Display a two dimensional color index array mapped to a disk using polar
   * coordinates.
   *
   * \param[in] x_org X coordinate of the disk center in world coordinates
   * \param[in] y_org Y coordinate of the disk center in world coordinates
   * \param[in] phimin start angle of the disk sector in degrees
   * \param[in] phimax end angle of the disk sector in degrees
   * \param[in] rmin inner radius of the punctured disk in world coordinates
   * \param[in] rmax outer radius of the disk in world coordinates
   * \param[in] dimphi Phi (X) dimension of the color index array
   * \param[in] dimr R (Y) dimension of the color index array
   * \param[in] scol number of leading columns in the color index array
   * \param[in] srow number of leading rows in the color index array
   * \param[in] ncol total number of columns in the color index array
   * \param[in] nrow total number of rows in the color index array
   * \param[in] color color index array
   *
   * The two dimensional color index array is mapped to the resulting image by
   * interpreting the X-axis of the array as the angle and the Y-axis as the radius.
   * The center point of the resulting disk is located at `x_org`, `y_org` and the
   * radius of the disk is `rmax`.
   *
   * To draw a contiguous array as a complete disk use:
   *
   *     gr_polarcellarray(x_org, y_org, 0, 360, 0, rmax, dimphi, dimr, 1, 1, dimphi, dimr, color)
   *
   * The additional parameters to the function can be used to further control the
   * mapping from polar to cartesian coordinates.
   *
   * If `rmin` is greater than 0 the input data is mapped to a punctured disk (or
   * annulus) with an inner radius of `rmin` and an outer radius `rmax`. If `rmin`
   * is greater than `rmax` the Y-axis of the array is reversed.
   *
   * The parameter `phimin` and `phimax` can be used to map the data to a sector
   * of the (punctured) disk starting at `phimin` and ending at `phimax`. If
   * `phimin` is greater than `phimax` the X-axis is reversed. The visible sector
   * is the one starting in mathematically positive direction (counterclockwise)
   * at the smaller angle and ending at the larger angle. An example of the four
   * possible options can be found below:
   *
   * \verbatim embed:rst:leading-asterisk
   *
   * +-----------+-----------+---------------------------------------------------+
   * |**phimin** |**phimax** |**Result**                                         |
   * +-----------+-----------+---------------------------------------------------+
   * |90         |270        |Left half visible, mapped counterclockwise         |
   * +-----------+-----------+---------------------------------------------------+
   * |270        |90         |Left half visible, mapped clockwise                |
   * +-----------+-----------+---------------------------------------------------+
   * |-90        |90         |Right half visible, mapped counterclockwise        |
   * +-----------+-----------+---------------------------------------------------+
   * |90         |-90        |Right half visible, mapped clockwise               |
   * +-----------+-----------+---------------------------------------------------+
   *
   * \endverbatim
   *
   * `scol` and `srow` can be used to specify a (1-based) starting column and row
   * in the `color` array. `ncol` and `nrow` specify the actual dimension of the
   * array in the memory whereof `dimphi` and `dimr` values are mapped to the disk.
   *
   */

  std::shared_ptr<GRM::Context> useContext = (extContext == nullptr) ? context : extContext;
  std::shared_ptr<GRM::Element> element =
      (extElement == nullptr) ? createElement("nonuniform_polarcellarray") : extElement;
  element->setAttribute("x_org", x_org);
  element->setAttribute("y_org", y_org);
  element->setAttribute("r", r_key);
  element->setAttribute("phi", phi_key);
  element->setAttribute("dimphi", dimphi);
  element->setAttribute("dimr", dimr);
  element->setAttribute("scol", scol);
  element->setAttribute("srow", srow);
  element->setAttribute("ncol", ncol);
  element->setAttribute("nrow", nrow);
  element->setAttribute("color", color_key);
  if (color != std::nullopt)
    {
      (*useContext)[color_key] = *color;
    }
  if (phi != std::nullopt)
    {
      (*useContext)[phi_key] = *phi;
    }
  if (r != std::nullopt)
    {
      (*useContext)[r_key] = *r;
    }
  return element;
}

std::shared_ptr<GRM::Element> GRM::Render::createNonUniformCellArray(
    const std::string &x_key, std::optional<std::vector<double>> x, const std::string &y_key,
    std::optional<std::vector<double>> y, int dimx, int dimy, int scol, int srow, int ncol, int nrow,
    const std::string &color_key, std::optional<std::vector<int>> color,
    const std::shared_ptr<GRM::Context> &extContext, const std::shared_ptr<GRM::Element> &extElement)
{
  /*!
   * This function can be used to create a non uniform cell array GRM::Element
   */
  std::shared_ptr<GRM::Context> useContext = (extContext == nullptr) ? context : extContext;
  std::shared_ptr<GRM::Element> element = (extElement == nullptr) ? createElement("nonuniformcellarray") : extElement;
  element->setAttribute("x", x_key);
  element->setAttribute("y", y_key);
  element->setAttribute("color", color_key);
  element->setAttribute("dimx", dimx);
  element->setAttribute("dimy", dimy);
  element->setAttribute("scol", scol);
  element->setAttribute("srow", srow);
  element->setAttribute("ncol", ncol);
  element->setAttribute("nrow", nrow);

  if (x != std::nullopt)
    {
      (*useContext)[x_key] = *x;
    }
  if (y != std::nullopt)
    {
      (*useContext)[y_key] = *y;
    }
  if (color != std::nullopt)
    {
      (*useContext)[color_key] = *color;
    }

  return element;
}

std::shared_ptr<GRM::Element> GRM::Render::createGrid3d(double x_tick, double y_tick, double z_tick, double x_org,
                                                        double y_org, double z_org, int x_major, int y_major,
                                                        int z_major, const std::shared_ptr<GRM::Element> &extElement)
{
  std::shared_ptr<GRM::Element> element = (extElement == nullptr) ? createElement("grid3d") : extElement;
  element->setAttribute("x_tick", x_tick);
  element->setAttribute("y_tick", y_tick);
  element->setAttribute("z_tick", z_tick);
  element->setAttribute("x_org", x_org);
  element->setAttribute("y_org", y_org);
  element->setAttribute("z_org", z_org);
  element->setAttribute("x_major", x_major);
  element->setAttribute("y_major", y_major);
  element->setAttribute("z_major", z_major);
  return element;
}

std::shared_ptr<GRM::Element> GRM::Render::createEmptyGrid3d(bool x_grid, bool y_grid, bool z_grid)
{
  auto element = createElement("grid3d");
  if (!x_grid)
    {
      element->setAttribute("x_tick", 0);
    }
  if (!y_grid)
    {
      element->setAttribute("y_tick", 0);
    }
  if (!z_grid)
    {
      element->setAttribute("z_tick", 0);
    }
  return element;
}

std::shared_ptr<GRM::Element> GRM::Render::createAxes3d(double x_tick, double y_tick, double z_tick, double x_org,
                                                        double y_org, double z_org, int major_x, int major_y,
                                                        int major_z, int tick_orientation,
                                                        const std::shared_ptr<GRM::Element> &extElement)
{
  std::shared_ptr<GRM::Element> element = (extElement == nullptr) ? createElement("axes3d") : extElement;
  element->setAttribute("x_tick", x_tick);
  element->setAttribute("y_tick", y_tick);
  element->setAttribute("z_tick", z_tick);
  element->setAttribute("x_org", x_org);
  element->setAttribute("y_org", y_org);
  element->setAttribute("z_org", z_org);
  element->setAttribute("major_x", major_x);
  element->setAttribute("major_y", major_y);
  element->setAttribute("major_z", major_z);
  element->setAttribute("tick_orientation", tick_orientation);
  return element;
}

std::shared_ptr<GRM::Element> GRM::Render::createEmptyAxes3d(int tick_orientation)
{
  auto element = createElement("axes3d");
  element->setAttribute("tick_orientation", tick_orientation);
  return element;
}

std::shared_ptr<GRM::Element>
GRM::Render::createPolyline3d(const std::string &x_key, std::optional<std::vector<double>> x, const std::string &y_key,
                              std::optional<std::vector<double>> y, const std::string &z_key,
                              std::optional<std::vector<double>> z, const std::shared_ptr<GRM::Context> &extContext,
                              const std::shared_ptr<GRM::Element> &extElement)
{
  std::shared_ptr<GRM::Context> useContext = (extContext == nullptr) ? context : extContext;
  std::shared_ptr<GRM::Element> element = (extElement == nullptr) ? createElement("polyline3d") : extElement;
  element->setAttribute("x", x_key);
  element->setAttribute("y", y_key);
  element->setAttribute("z", z_key);

  if (x != std::nullopt)
    {
      (*useContext)[x_key] = *x;
    }
  if (y != std::nullopt)
    {
      (*useContext)[y_key] = *y;
    }
  if (z != std::nullopt)
    {
      (*useContext)[z_key] = *z;
    }

  return element;
}

std::shared_ptr<GRM::Element> GRM::Render::createPolymarker3d(
    const std::string &x_key, std::optional<std::vector<double>> x, const std::string &y_key,
    std::optional<std::vector<double>> y, const std::string &z_key, std::optional<std::vector<double>> z,
    const std::shared_ptr<GRM::Context> &extContext, const std::shared_ptr<GRM::Element> &extElement)
{
  std::shared_ptr<GRM::Context> useContext = (extContext == nullptr) ? context : extContext;
  std::shared_ptr<GRM::Element> element = (extElement == nullptr) ? createElement("polymarker3d") : extElement;
  element->setAttribute("x", x_key);
  element->setAttribute("y", y_key);
  element->setAttribute("z", z_key);

  if (x != std::nullopt)
    {
      (*useContext)[x_key] = *x;
    }
  if (y != std::nullopt)
    {
      (*useContext)[y_key] = *y;
    }
  if (z != std::nullopt)
    {
      (*useContext)[z_key] = *z;
    }

  return element;
}

std::shared_ptr<GRM::Element> GRM::Render::createGR3DrawMesh(
    int mesh, int n, const std::string &positions_key, std::optional<std::vector<double>> positions,
    const std::string &directions_key, std::optional<std::vector<double>> directions, const std::string &ups_key,
    std::optional<std::vector<double>> ups, const std::string &colors_key, std::optional<std::vector<double>> colors,
    const std::string &scales_key, std::optional<std::vector<double>> scales,
    const std::shared_ptr<GRM::Context> &extContext, const std::shared_ptr<GRM::Element> &extElement)
{

  std::shared_ptr<GRM::Context> useContext = (extContext == nullptr) ? context : extContext;
  std::shared_ptr<GRM::Element> element = (extElement == nullptr) ? createElement("gr3drawmesh") : extElement;
  element->setAttribute("mesh", mesh);
  element->setAttribute("n", n);
  element->setAttribute("positions", positions_key);
  element->setAttribute("directions", directions_key);
  element->setAttribute("ups", ups_key);
  element->setAttribute("colors", colors_key);
  element->setAttribute("scales", scales_key);

  if (positions != std::nullopt)
    {
      (*useContext)[positions_key] = *positions;
    }
  if (directions != std::nullopt)
    {
      (*useContext)[directions_key] = *directions;
    }
  if (ups != std::nullopt)
    {
      (*useContext)[ups_key] = *ups;
    }
  if (colors != std::nullopt)
    {
      (*useContext)[colors_key] = *colors;
    }
  if (scales != std::nullopt)
    {
      (*useContext)[scales_key] = *scales;
    }
  return element;
}

std::shared_ptr<GRM::Element>
GRM::Render::createTriSurface(const std::string &px_key, std::optional<std::vector<double>> px,
                              const std::string &py_key, std::optional<std::vector<double>> py,
                              const std::string &pz_key, std::optional<std::vector<double>> pz,
                              const std::shared_ptr<GRM::Context> &extContext)
{
  std::shared_ptr<GRM::Context> useContext = (extContext == nullptr) ? context : extContext;
  auto element = createSeries("trisurface");
  element->setAttribute("x", px_key);
  element->setAttribute("y", py_key);
  element->setAttribute("z", pz_key);

  if (px != std::nullopt)
    {
      (*useContext)[px_key] = *px;
    }
  if (py != std::nullopt)
    {
      (*useContext)[py_key] = *py;
    }
  if (pz != std::nullopt)
    {
      (*useContext)[pz_key] = *pz;
    }

  return element;
}

std::shared_ptr<GRM::Element> GRM::Render::createTitles3d(const std::string &x, const std::string &y,
                                                          const std::string &z,
                                                          const std::shared_ptr<GRM::Element> &extElement)
{
  std::shared_ptr<GRM::Element> element = (extElement == nullptr) ? createElement("titles3d") : extElement;
  element->setAttribute("x", x);
  element->setAttribute("y", y);
  element->setAttribute("z", z);

  return element;
}

std::shared_ptr<GRM::Element> GRM::Render::createGR3DrawImage(double xmin, double xmax, double ymin, double ymax,
                                                              int width, int height, int drawable_type,
                                                              const std::shared_ptr<GRM::Element> &extElement)
{
  std::shared_ptr<GRM::Element> element = (extElement == nullptr) ? createElement("gr3drawimage") : extElement;
  element->setAttribute("xmin", xmin);
  element->setAttribute("xmax", xmax);
  element->setAttribute("ymin", ymin);
  element->setAttribute("ymax", ymax);
  element->setAttribute("width", width);
  element->setAttribute("height", height);
  element->setAttribute("drawable_type", drawable_type);
  return element;
}

std::shared_ptr<GRM::Element> GRM::Render::createDrawGraphics(const std::string &data_key,
                                                              std::optional<std::vector<int>> data,
                                                              const std::shared_ptr<GRM::Context> &extContext,
                                                              const std::shared_ptr<GRM::Element> &extElement)
{
  std::shared_ptr<GRM::Context> useContext = (extContext == nullptr) ? context : extContext;
  std::shared_ptr<GRM::Element> element = (extElement == nullptr) ? createElement("drawgraphics") : extElement;

  if (data != std::nullopt)
    {
      (*useContext)[data_key] = *data;
    }
  element->setAttribute("data", data_key);
  return element;
}

std::shared_ptr<GRM::Element> GRM::Render::createIsoSurfaceRenderElement(int drawable_type)
{
  auto element = createElement("isosurface_render");
  element->setAttribute("drawable_type", drawable_type);
  return element;
}

std::shared_ptr<GRM::Element> GRM::Render::createLayoutGrid(const grm::Grid &grid)
{
  auto element = createElement("layout_grid");

  element->setAttribute("absHeight", grid.absHeight);
  element->setAttribute("absWidth", grid.absWidth);
  element->setAttribute("absHeightPxl", grid.absHeightPxl);
  element->setAttribute("absWidthPxl", grid.absWidthPxl);
  element->setAttribute("fitParentsHeight", grid.fitParentsHeight);
  element->setAttribute("fitParentsWidth", grid.fitParentsWidth);
  element->setAttribute("relativeHeight", grid.relativeHeight);
  element->setAttribute("relativeWidth", grid.relativeWidth);
  element->setAttribute("aspectRatio", grid.aspectRatio);
  element->setAttribute("widthSet", grid.widthSet);
  element->setAttribute("heightSet", grid.heightSet);
  element->setAttribute("arSet", grid.arSet);
  element->setAttribute("nrows", grid.getNRows());
  element->setAttribute("ncols", grid.getNCols());

  return element;
}

std::shared_ptr<GRM::Element> GRM::Render::createLayoutGridElement(const grm::GridElement &gridElement,
                                                                   const grm::Slice &slice)
{
  auto element = createElement("layout_gridelement");

  element->setAttribute("absHeight", gridElement.absHeight);
  element->setAttribute("absWidth", gridElement.absWidth);
  element->setAttribute("absHeightPxl", gridElement.absHeightPxl);
  element->setAttribute("absWidthPxl", gridElement.absWidthPxl);
  element->setAttribute("fitParentsHeight", gridElement.fitParentsHeight);
  element->setAttribute("fitParentsWidth", gridElement.fitParentsWidth);
  element->setAttribute("relativeHeight", gridElement.relativeHeight);
  element->setAttribute("relativeWidth", gridElement.relativeWidth);
  element->setAttribute("aspectRatio", gridElement.aspectRatio);
  element->setAttribute("rowStart", slice.rowStart);
  element->setAttribute("rowStop", slice.rowStop);
  element->setAttribute("colStart", slice.colStart);
  element->setAttribute("colStop", slice.colStop);

  double *subplot = gridElement.subplot;
  GRM::Render::setSubplot(element, subplot[0], subplot[1], subplot[2], subplot[3]);

  return element;
}

std::shared_ptr<GRM::Element> GRM::Render::createPanzoom(double x, double y, double xzoom, double yzoom)
{
  auto element = createElement("panzoom");
  element->setAttribute("x", x);
  element->setAttribute("y", y);
  element->setAttribute("xzoom", xzoom);
  element->setAttribute("yzoom", yzoom);
  return element;
}

std::shared_ptr<GRM::Element> GRM::Render::createPolarBar(double count, int class_nr,
                                                          const std::shared_ptr<GRM::Element> &extElement)
{
  std::shared_ptr<GRM::Element> element = (extElement == nullptr) ? createElement("polar_bar") : extElement;
  element->setAttribute("count", count);
  element->setAttribute("class_nr", class_nr);

  return element;
}

std::shared_ptr<GRM::Element> GRM::Render::createErrorbar(double errorbar_x, double errorbar_ymin, double errorbar_ymax,
                                                          int color_errorbar,
                                                          const std::shared_ptr<GRM::Element> &extElement)
{
  std::shared_ptr<GRM::Element> element = (extElement == nullptr) ? createElement("errorbar") : extElement;
  element->setAttribute("errorbar_x", errorbar_x);
  element->setAttribute("errorbar_ymin", errorbar_ymin);
  element->setAttribute("errorbar_ymax", errorbar_ymax);
  element->setAttribute("color_errorbar", color_errorbar);

  return element;
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~ modifier functions~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 */

void GRM::Render::setViewport(const std::shared_ptr<GRM::Element> &element, double xmin, double xmax, double ymin,
                              double ymax)
{
  /*!
   * This function can be used to set the viewport of a GRM::Element
   *
   * \param[in] element A GRM::Element
   * \param[in] xmin The left horizontal coordinate of the viewport (0 <= xmin < xmax)
   * \param[in] xmax The right horizontal coordinate of the viewport (xmin < xmax <= 1)
   * \param[in] ymin TThe bottom vertical coordinate of the viewport (0 <= ymin < ymax)
   * \param[in] ymax The top vertical coordinate of the viewport (ymin < ymax <= 1)
   */

  element->setAttribute("viewport_xmin", xmin);
  element->setAttribute("viewport_xmax", xmax);
  element->setAttribute("viewport_ymin", ymin);
  element->setAttribute("viewport_ymax", ymax);
}


void GRM::Render::setWSViewport(const std::shared_ptr<GRM::Element> &element, double xmin, double xmax, double ymin,
                                double ymax)
{
  /*!
   * This function can be used to set the wsviewport of a GRM::Element
   *
   * \param[in] element A GRM::Element
   * \param[in] xmin The left horizontal coordinate of the viewport (0 <= xmin < xmax)
   * \param[in] xmax The right horizontal coordinate of the viewport (xmin < xmax <= 1)
   * \param[in] ymin TThe bottom vertical coordinate of the viewport (0 <= ymin < ymax)
   * \param[in] ymax The top vertical coordinate of the viewport (ymin < ymax <= 1)
   */

  element->setAttribute("wsviewport_xmin", xmin);
  element->setAttribute("wsviewport_xmax", xmax);
  element->setAttribute("wsviewport_ymin", ymin);
  element->setAttribute("wsviewport_ymax", ymax);
}

void GRM::Render::setWindow(const std::shared_ptr<Element> &element, double xmin, double xmax, double ymin, double ymax)
{
  /*!
   * This function can be used to set the window of a GRM::Element
   *
   * \param[in] element A GRM::Element
   * \param[in] xmin The left horizontal coordinate of the window (xmin < xmax)
   * \param[in] xmax The right horizontal coordinate of the window (xmin < xmax)
   * \param[in] ymin The bottom vertical coordinate of the window (ymin < ymax)
   * \param[in] ymax The top vertical coordinate of the window (ymin < ymax)
   */

  element->setAttribute("window_xmin", xmin);
  element->setAttribute("window_xmax", xmax);
  element->setAttribute("window_ymin", ymin);
  element->setAttribute("window_ymax", ymax);
}

void GRM::Render::setWSWindow(const std::shared_ptr<Element> &element, double xmin, double xmax, double ymin,
                              double ymax)
{
  /*!
   * This function can be used to set the ws_window of a GRM::Element
   *
   * \param[in] element A GRM::Element
   * \param[in] xmin The left horizontal coordinate of the window (xmin < xmax)
   * \param[in] xmax The right horizontal coordinate of the window (xmin < xmax)
   * \param[in] ymin The bottom vertical coordinate of the window (ymin < ymax)
   * \param[in] ymax The top vertical coordinate of the window (ymin < ymax)
   */

  element->setAttribute("wswindow_xmin", xmin);
  element->setAttribute("wswindow_xmax", xmax);
  element->setAttribute("wswindow_ymin", ymin);
  element->setAttribute("wswindow_ymax", ymax);
}

void GRM::Render::setMarkerType(const std::shared_ptr<Element> &element, int type)
{
  /*!
   * This function can be used to set a MarkerType of a GRM::Element
   *
   * \param[in] element A GRM::Element
   * \param[in] type An Integer setting the MarkerType
   */
  element->setAttribute("markertype", type);
}

void GRM::Render::setMarkerType(const std::shared_ptr<Element> &element, const std::string &types_key,
                                std::optional<std::vector<int>> types, const std::shared_ptr<GRM::Context> &extContext)
{
  /*!
   * This function can be used to set a vector of MarkerTypes of a GRM::Element
   *
   * \param[in] element A GRM::Element
   * \param[in] types_key A string used as a key for storing the types
   * \param[in] types A vector containing the MarkerTypes
   * \param[in] extContext A GRM::Context used for storing types. By default it uses GRM::Render's GRM::Context object
   * but an external GRM::Context can be used
   */
  std::shared_ptr<GRM::Context> useContext = (extContext == nullptr) ? context : extContext;
  if (types != std::nullopt)
    {
      (*useContext)[types_key] = *types;
    }
  element->setAttribute("markertypes", types_key);
}

void GRM::Render::setMarkerSize(const std::shared_ptr<Element> &element, double size)
{
  /*!
   * This function can be used to set a MarkerSize of a GRM::Element
   *
   * \param[in] element A GRM::Element
   * \param[in] type A Double setting the MarkerSize
   */
  element->setAttribute("markersize", size);
}

void GRM::Render::setMarkerSize(const std::shared_ptr<Element> &element, const std::string &sizes_key,
                                std::optional<std::vector<double>> sizes,
                                const std::shared_ptr<GRM::Context> &extContext)
{
  /*!
   * This function can be used to set a vector of MarkerTypes of a GRM::Element
   *
   * \param[in] element A GRM::Element
   * \param[in] sizes_key A string used as a key for storing the sizes
   * \param[in] sizes A vector containing the MarkerSizes
   * \param[in] extContext A GRM::Context used for storing sizes. By default it uses GRM::Render's GRM::Context object
   * but an external GRM::Context can be used
   */
  std::shared_ptr<GRM::Context> useContext = (extContext == nullptr) ? context : extContext;
  if (sizes != std::nullopt)
    {
      (*useContext)[sizes_key] = *sizes;
    }
  element->setAttribute("markersizes", sizes_key);
}

void GRM::Render::setMarkerColorInd(const std::shared_ptr<Element> &element, int color)
{
  /*!
   * This function can be used to set a MarkerColorInd of a GRM::Element
   *
   * \param[in] element A GRM::Element
   * \param[in] color An Integer setting the MarkerColorInd
   */
  element->setAttribute("markercolorind", color);
}

void GRM::Render::setMarkerColorInd(const std::shared_ptr<Element> &element, const std::string &colorinds_key,
                                    std::optional<std::vector<int>> colorinds,
                                    const std::shared_ptr<GRM::Context> &extContext)
{
  /*!
   * This function can be used to set a vector of MarkerColorInds of a GRM::Element
   *
   * \param[in] element A GRM::Element
   * \param[in] colorinds_key A string used as a key for storing the colorinds
   * \param[in] colorinds A vector containing the MarkerColorInds
   * \param[in] extContext A GRM::Context used for storing colorinds. By default it uses GRM::Render's GRM::Context
   * object but an external GRM::Context can be used
   */
  std::shared_ptr<GRM::Context> useContext = (extContext == nullptr) ? context : extContext;
  if (colorinds != std::nullopt)
    {
      (*useContext)[colorinds_key] = *colorinds;
    }
  element->setAttribute("markercolorinds", colorinds_key);
}

void GRM::Render::setLineType(const std::shared_ptr<Element> &element, const std::string &types_key,
                              std::optional<std::vector<int>> types, const std::shared_ptr<GRM::Context> &extContext)
{
  /*!
   * This function can be used to set a vector of LineTypes of a GRM::Element
   *
   * \param[in] element A GRM::Element
   * \param[in] types_key A string used as a key for storing the types
   * \param[in] types A vector containing the LineTypes
   * \param[in] extContext A GRM::Context used for storing types. By default it uses GRM::Render's GRM::Context object
   * but an external GRM::Context can be used
   */
  std::shared_ptr<GRM::Context> useContext = (extContext == nullptr) ? context : extContext;
  if (types != std::nullopt)
    {
      (*useContext)[types_key] = *types;
    }
  element->setAttribute("linetypes", types_key);
}

void GRM::Render::setLineType(const std::shared_ptr<Element> &element, int type)
{
  /*!
   * This function can be used to set a LineType of a GRM::Element
   *
   * \param[in] element A GRM::Element
   * \param[in] type An Integer setting the LineType
   */
  element->setAttribute("linetype", type);
}

void GRM::Render::setLineWidth(const std::shared_ptr<Element> &element, const std::string &widths_key,
                               std::optional<std::vector<double>> widths,
                               const std::shared_ptr<GRM::Context> &extContext)
{
  /*!
   * This function can be used to set a vector of LineWidths of a GRM::Element
   *
   * \param[in] element A GRM::Element
   * \param[in] widths_key A string used as a key for storing the widths
   * \param[in] widths A vector containing the LineWidths
   * \param[in] extContext A GRM::Context used for storing widths. By default it uses GRM::Render's GRM::Context
   * object but an external GRM::Context can be used
   */
  std::shared_ptr<GRM::Context> useContext = (extContext == nullptr) ? context : extContext;
  if (widths != std::nullopt)
    {
      (*useContext)[widths_key] = *widths;
    }
  element->setAttribute("linewidths", widths_key);
}

void GRM::Render::setLineWidth(const std::shared_ptr<Element> &element, double width)
{
  /*!
   * This function can be used to set a LineWidth of a GRM::Element
   *
   * \param[in] element A GRM::Element
   * \param[in] type A Double setting the LineWidth
   */
  element->setAttribute("linewidth", width);
}

void GRM::Render::setLineColorInd(const std::shared_ptr<Element> &element, const std::string &colorinds_key,
                                  std::optional<std::vector<int>> colorinds,
                                  const std::shared_ptr<GRM::Context> &extContext)
{
  /*!
   * This funciton can be used to set a vector of LineColorInds of a GRM::Element
   *
   * \param[in] element A GRM::Element
   * \param[in] colorinds_key A string used as a key for storing the colorinds
   * \param[in] colorinds A vector containing the Colorinds
   * \param[in] extContext A GRM::Context used for storing colorinds. By default it uses GRM::Render's GRM::Context
   * object but an external GRM::Context can be used
   */
  std::shared_ptr<GRM::Context> useContext = (extContext == nullptr) ? context : extContext;
  if (colorinds != std::nullopt)
    {
      (*useContext)[colorinds_key] = *colorinds;
    }
  element->setAttribute("linecolorinds", colorinds_key);
}

void GRM::Render::setLineColorInd(const std::shared_ptr<Element> &element, int color)
{
  /*!
   * This function can be used to set LineColorInd of a GRM::Element
   *
   * \param[in] element A GRM::Element
   * \param[in] color An Integer value setting the LineColorInd
   */
  element->setAttribute("linecolorind", color);
}

void GRM::Render::setTextFontPrec(const std::shared_ptr<Element> &element, int font, int prec)
{
  /*!
   * This function can be used to set TextFontPrec of a GRM::Element
   *
   * \param[in] element A GRM::Element
   * \param[in] font An Integer value representing a font type
   * \param[in] prec An Integer value representing a font precision
   */

  element->setAttribute("textfontprec_font", font);
  element->setAttribute("textfontprec_prec", prec);
}

void GRM::Render::setCharUp(const std::shared_ptr<Element> &element, double ux, double uy)
{
  /*!
   * This function can be used to set CharUp of a GRM::Element
   *
   * \param[in] element A GRM::Element
   * \param[in] ux  X coordinate of the text up vector
   * \param[in] uy  y coordinate of the text up vector
   */

  element->setAttribute("charup_x", ux);
  element->setAttribute("charup_y", uy);
}

void GRM::Render::setTextAlign(const std::shared_ptr<Element> &element, int horizontal, int vertical)
{
  /*!
   * This function can be used to set TextAlign of a GRM::Element
   *
   * \param[in] element A GRM::Element
   * \param[in] horizontal  Horizontal text alignment
   * \param[in] vertical Vertical text alignment
   */
  element->setAttribute("textalign_horizontal", horizontal);
  element->setAttribute("textalign_vertical", vertical);
}

void GRM::Render::setTextWidthAndHeight(const std::shared_ptr<Element> &element, double width, double height)
{
  /*!
   * This function can be used to set the width and height of a GRM::Element
   *
   * \param[in] element A GRM::Element
   * \param[in] width Width of the Element
   * \param[in] height Height of the Element
   */
  element->setAttribute("width", width);
  element->setAttribute("height", height);
}

void GRM::Render::setLineSpec(const std::shared_ptr<Element> &element, const std::string &spec)
{
  /*!
   * This function can be used to set the linespec of a GRM::Element
   *
   * \param[in] element A GRM::Element
   * \param[in] spec An std::string
   *
   */
  element->setAttribute("linespec", spec);
}

void GRM::Render::setColorRep(const std::shared_ptr<Element> &element, int index, double red, double green, double blue)
{
  /*!
   * This function can be used to set the colorrep of a GRM::Element
   *
   * \param[in] element A GRM::Element
   * \param[in] index Color index in the range 0 to 1256
   * \param[in] red Red intensity in the range 0.0 to 1.0
   * \param[in] green Green intensity in the range 0.0 to 1.0
   * \param[in] blue Blue intensity in the range 0.0 to 1.0
   */

  int precision = 255;
  int red_int = int(red * precision + 0.5), green_int = int(green * precision + 0.5),
      blue_int = int(blue * precision + 0.5);

  // Convert RGB to hex
  std::stringstream stream;
  std::string hex;
  stream << std::hex << (red_int << 16 | green_int << 8 | blue_int);

  std::string name = "colorrep." + std::to_string(index);

  element->setAttribute(name, stream.str());
}

void GRM::Render::setFillIntStyle(const std::shared_ptr<GRM::Element> &element, int index)
{
  /*!
   * This function can be used to set the fillintstyle of a GRM::Element
   *
   * \param[in] element A GRM::Element
   * \param[in] index The style of fill to be used
   */
  element->setAttribute("fillintstyle", index);
}

void GRM::Render::setFillColorInd(const std::shared_ptr<GRM::Element> &element, int color)
{
  /*!
   * This function can be used to set the fillcolorind of a GRM::Element
   *
   * \param[in] element A GRM::Element
   * \param[in] color The fill area color index (COLOR < 1256)
   */
  element->setAttribute("fillcolorind", color);
}

void GRM::Render::setFillStyle(const std::shared_ptr<GRM::Element> &element, int index)
{
  /*!
   * This function can be used to set the fillintstyle of a GRM::Element
   *
   * \param[in] element A GRM::Element
   * \param[in] index The fill style index to be used
   */

  element->setAttribute("fillstyle", index);
}

void GRM::Render::setScale(const std::shared_ptr<GRM::Element> &element, int scale)
{
  /*!
   * This function can be used to set the scale of a GRM::Element
   *
   * \param[in] element A GRM::Element
   * \param[in] index The scale index to be used
   */
  element->setAttribute("scale", scale);
}

void GRM::Render::setWindow3d(const std::shared_ptr<GRM::Element> &element, double xmin, double xmax, double ymin,
                              double ymax, double zmin, double zmax)
{
  /*!
   * This function can be used to set the window3d of a GRM::Element
   *
   * \param[in] element A GRM::Element
   * \param[in] xmin The left horizontal coordinate of the window (xmin < xmax)
   * \param[in] xmax The right horizontal coordinate of the window (xmin < xmax)
   * \param[in] ymin The bottom vertical coordinate of the window (ymin < ymax)
   * \param[in] ymax The top vertical coordinate of the window (ymin < ymax)
   * \param[in] zmin min z-value
   * \param[in] zmax max z-value
   */

  element->setAttribute("window_xmin", xmin);
  element->setAttribute("window_xmax", xmax);
  element->setAttribute("window_ymin", ymin);
  element->setAttribute("window_ymax", ymax);
  element->setAttribute("window_zmin", zmin);
  element->setAttribute("window_zmax", zmax);
}

void GRM::Render::setSpace3d(const std::shared_ptr<GRM::Element> &element, double fov, double camera_distance)
{
  /*! This function can be used to set the space3d of a GRM::Element
   *
   * \param[in] element A GRM::Element
   * \param[in] phi: azimuthal angle of the spherical coordinates
   * \param[in] theta: polar angle of the spherical coordinates
   * \param[in] fov: vertical field of view(0 or NaN for orthographic projection)
   * \param[in] camera_distance: distance between the camera and the focus point (in arbitrary units, 0 or NaN for the
   * radius of the object's smallest bounding sphere)
   */

  element->setAttribute("space3d_fov", fov);
  element->setAttribute("space3d_camera_distance", camera_distance);
}

void GRM::Render::setSpace(const std::shared_ptr<Element> &element, double zmin, double zmax, int rotation, int tilt)
{
  /*!
   * This function can be used to set the space of a GRM::Element
   *
   * \param[in] element A GRM::Element
   * \param[in] zmin
   * \param[in] zmax
   * \param[in] rotation
   * \param[in] tilt
   */

  element->setAttribute("space", true);
  element->setAttribute("space_zmin", zmin);
  element->setAttribute("space_zmax", zmax);
  element->setAttribute("space_rotation", rotation);
  element->setAttribute("space_tilt", tilt);
}

void GRM::Render::setSelntran(const std::shared_ptr<Element> &element, int transform)
{
  /*! This function can be used to set the window3d of a GRM::Element
   *
   * \param[in] element A GRM::Element
   * \param[in] transform Select a predefined transformation from world coordinates to normalized device coordinates.
   */

  element->setAttribute("selntran", transform);
}

void GRM::Render::setGR3BackgroundColor(const std::shared_ptr<GRM::Element> &element, double red, double green,
                                        double blue, double alpha)
{
  /*! This function can be used to set the gr3 backgroundcolor of a GRM::Element
   *
   * \param[in] element A GRM::Element
   */

  element->setAttribute("gr3backgroundcolor_red", red);
  element->setAttribute("gr3backgroundcolor_green", green);
  element->setAttribute("gr3backgroundcolor_blue", blue);
  element->setAttribute("gr3backgroundcolor_alpha", alpha);
}

void GRM::Render::setGR3CameraLookAt(const std::shared_ptr<GRM::Element> &element, double camera_x, double camera_y,
                                     double camera_z, double center_x, double center_y, double center_z, double up_x,
                                     double up_y, double up_z)
{
  /*! This function can be used to set the gr3 camerlookat of a GRM::Element
   *
   * \param[in] element A GRM::Element
   * \param[in] camera_x: The x-coordinate of the camera
   * \param[in] camera_y: The y-coordinate of the camera
   * \param[in] camera_z: The z-coordinate of the camera
   * \param[in] center_x: The x-coordinate of the center of focus
   * \param[in] center_y: The y-coordinate of the center of focus
   * \param[in] center_z: The z-coordinate of the center of focus
   * \param[in] up_x: The x-component of the up direction
   * \param[in] up_y: The y-component of the up direction
   * \param[in] up_z: The z-component of the up direction
   */

  element->setAttribute("gr3cameralookat_camera_x", camera_x);
  element->setAttribute("gr3cameralookat_camera_y", camera_y);
  element->setAttribute("gr3cameralookat_camera_z", camera_z);
  element->setAttribute("gr3cameralookat_center_x", center_x);
  element->setAttribute("gr3cameralookat_center_y", center_y);
  element->setAttribute("gr3cameralookat_center_z", center_z);
  element->setAttribute("gr3cameralookat_up_x", up_x);
  element->setAttribute("gr3cameralookat_up_y", up_y);
  element->setAttribute("gr3cameralookat_up_z", up_z);
}

void GRM::Render::setTextColorInd(const std::shared_ptr<GRM::Element> &element, int index)
{
  /*!
   * This function can be used to set the textcolorind of a GRM::Element
   * \param[in] element A GRM::Element
   * \param[in] index The color index
   */

  element->setAttribute("textcolorind", index);
}

void GRM::Render::setBorderColorInd(const std::shared_ptr<GRM::Element> &element, int index)
{
  /*!
   * This function can be used to set the bordercolorind of a GRM::Element
   * \param[in] element A GRM::Element
   * \param[in] index The color index
   */
  element->setAttribute("bordercolorind", index);
}

void GRM::Render::selectClipXForm(const std::shared_ptr<GRM::Element> &element, int form)
{
  /*!
   * This function can be used to set the clipxform of a GRM::Element
   * \param[in] element A GRM::Element
   * \param[in] form the clipxform
   */
  element->setAttribute("clipxform", form);
}

void GRM::Render::setCharHeight(const std::shared_ptr<GRM::Element> &element, double height)
{
  /*!
   * This function can be used to set the charheight of a GRM::Element
   * \param[in] element A GRM::Element
   * \param[in] height the charheight
   */
  element->setAttribute("charheight", height);
}

void GRM::Render::setProjectionType(const std::shared_ptr<GRM::Element> &element, int type)
{
  /*!
   * This function can be used to set the projectiontype of a GRM::Element
   *
   * \param[in] element A GRM::Element
   * \param[in] type The projectiontype
   */
  element->setAttribute("projectiontype", type);
}

void GRM::Render::setTransparency(const std::shared_ptr<GRM::Element> &element, double alpha)
{
  /*!
   * This function can be used to set the transparency of a GRM::Element
   * \param[in] element A GRM::Element
   * \param[in] alpha The alpha
   */
  element->setAttribute("transparency", alpha);
}

void GRM::Render::setResampleMethod(const std::shared_ptr<GRM::Element> &element, int resample)
{
  /*!
   * This function can be used to set the resamplemethod of a GRM::Element
   * \param[in] element A GRM::Element
   * \param[in] resample The resample method
   */

  element->setAttribute("resample_method", resample);
}

void GRM::Render::setTextEncoding(const std::shared_ptr<Element> &element, int encoding)
{
  /*!
   * This function can be used to set the textencoding of a GRM::Element
   * \param[in] element A GRM::Element
   * \param[in] encoding The textencoding
   */
  element->setAttribute("textencoding", encoding);
}

void GRM::Render::setSubplot(const std::shared_ptr<GRM::Element> &element, double xmin, double xmax, double ymin,
                             double ymax)
{
  element->setAttribute("plot_xmin", xmin);
  element->setAttribute("plot_xmax", xmax);
  element->setAttribute("plot_ymin", ymin);
  element->setAttribute("plot_ymax", ymax);
}

void GRM::Render::setXTickLabels(std::shared_ptr<GRM::Element> element, const std::string &key,
                                 std::optional<std::vector<std::string>> xticklabels,
                                 const std::shared_ptr<GRM::Context> &extContext)
{
  /*!
   * This function can be used to create a XTickLabel GRM::Element
   *
   * \param[in] key A string used for storing the xticklabels in GRM::Context
   * \param[in] xticklabels A vector containing string values representing xticklabels
   * \param[in] extContext A GRM::Context that is used for storing the vectors. By default it uses GRM::Render's
   * GRM::Context object but an external GRM::Context can be used
   */

  std::shared_ptr<GRM::Context> useContext = (extContext == nullptr) ? context : extContext;
  if (xticklabels != std::nullopt)
    {
      (*useContext)[key] = *xticklabels;
    }
  element->setAttribute("xticklabels", key);
}

void GRM::Render::setYTickLabels(std::shared_ptr<GRM::Element> element, const std::string &key,
                                 std::optional<std::vector<std::string>> yticklabels,
                                 const std::shared_ptr<GRM::Context> &extContext)
{
  /*!
   * This function can be used to create a YTickLabel GRM::Element
   *
   * \param[in] key A string used for storing the yticklabels in GRM::Context
   * \param[in] yticklabels A vector containing string values representing yticklabels
   * \param[in] extContext A GRM::Context that is used for storing the vectors. By default it uses GRM::Render's
   * GRM::Context object but an external GRM::Context can be used
   */

  std::shared_ptr<GRM::Context> useContext = (extContext == nullptr) ? context : extContext;
  if (yticklabels != std::nullopt)
    {
      (*useContext)[key] = *yticklabels;
    }
  element->setAttribute("yticklabels", key);
}

void GRM::Render::setNextColor(const std::shared_ptr<GRM::Element> &element, const std::string &color_indices_key,
                               const std::vector<int> &color_indices, const std::shared_ptr<GRM::Context> &extContext)
{
  auto useContext = (extContext == nullptr) ? context : extContext;
  element->setAttribute("setNextColor", 1);
  if (!color_indices.empty())
    {
      (*useContext)[color_indices_key] = color_indices;
      element->setAttribute("color_indices", color_indices_key);
    }
  else
    {
      throw NotFoundError("Color indices are missing in vector\n");
    }
}

void GRM::Render::setNextColor(const std::shared_ptr<GRM::Element> &element, const std::string &color_rgb_values_key,
                               const std::vector<double> &color_rgb_values,
                               const std::shared_ptr<GRM::Context> &extContext)
{
  auto useContext = (extContext == nullptr) ? context : extContext;
  element->setAttribute("setNextColor", true);
  if (!color_rgb_values.empty())
    {
      (*useContext)[color_rgb_values_key] = color_rgb_values;
      element->setAttribute("color_rgb_values", color_rgb_values_key);
    }
}

void GRM::Render::setNextColor(const std::shared_ptr<GRM::Element> &element,
                               std::optional<std::string> color_indices_key,
                               std::optional<std::string> color_rgb_values_key)
{
  if (color_indices_key != std::nullopt)
    {
      element->setAttribute("color_indices", (*color_indices_key));
      element->setAttribute("setNextColor", true);
    }
  else if (color_rgb_values_key != std::nullopt)
    {
      element->setAttribute("setNextColor", true);
      element->setAttribute("color_rgb_values", (*color_rgb_values_key));
    }
}

void GRM::Render::setNextColor(const std::shared_ptr<GRM::Element> &element)
{
  element->setAttribute("setNextColor", true);
  element->setAttribute("snc_fallback", true);
}

void GRM::Render::setOriginPosition(const std::shared_ptr<GRM::Element> &element, std::string x_org_pos,
                                    std::string y_org_pos)
{
  element->setAttribute("x_org_pos", x_org_pos);
  element->setAttribute("y_org_pos", y_org_pos);
}

void GRM::Render::setOriginPosition3d(const std::shared_ptr<GRM::Element> &element, std::string x_org_pos,
                                      std::string y_org_pos, std::string z_org_pos)
{
  setOriginPosition(element, x_org_pos, y_org_pos);
  element->setAttribute("z_org_pos", z_org_pos);
}

void GRM::Render::setGR3LightParameters(const std::shared_ptr<GRM::Element> &element, double ambient, double diffuse,
                                        double specular, double specular_power)
{
  element->setAttribute("ambient", ambient);
  element->setAttribute("diffuse", diffuse);
  element->setAttribute("specular", specular);
  element->setAttribute("specular_power", specular_power);
}

void GRM::Render::setAutoUpdate(bool update)
{
  automatic_update = update;
}

void GRM::Render::getAutoUpdate(bool *update)
{
  *update = automatic_update;
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~ filter functions~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 */

void updateFilter(const std::shared_ptr<GRM::Element> &element, const std::string &attr, const std::string &value = "")
{
  std::vector<std::string> bar{
      "bar_color_index", "bar_color_rgb", "edge_color_index", "edge_color_rgb", "text", "x1", "x2", "y1", "y2",
  };
  std::vector<std::string> errorbar{
      "e_downwards", "e_upwards", "errorbar_x", "errorbar_ymax", "errorbar_ymin", "scap_xmax", "scap_xmin",
  };
  std::vector<std::string> polar_bar{
      "bin_width",  "bin_widths", "bin_edges", "class_nr",  "count",
      "draw_edges", "norm",       "phiflip",   "xcolormap", "ycolormap",
  };
  std::vector<std::string> series_barplot{
      "bar_color",      "bar_width",      "c",       "clipxform",    "edge_color",  "ind_bar_color",
      "ind_edge_color", "ind_edge_width", "indices", "inner_series", "orientation", "rgb",
      "style",          "width",          "y",       "ylabels",
  };
  std::vector<std::string> series_contour{
      "levels", "px", "py", "pz", "x", "y", "z",
  };
  std::vector<std::string> series_contourf = series_contour;
  std::vector<std::string> series_heatmap{
      "x", "y", "z", "zrange_max", "zrange_min",
  };
  std::vector<std::string> series_hexbin{
      "nbins",
      "x",
      "y",
  };
  std::vector<std::string> series_hist{
      "bar_color_index", "bar_color_rgb", "bins", "edge_color_index", "edge_color_rgb", "orientation",
  };
  std::vector<std::string> series_imshow{
      "c", "c_dims", "img_data_key", "x", "y", "z",
  };
  std::vector<std::string> series_isosurface{
      "ambient", "c", "c_dims", "diffuse", "foreground_color", "isovalue", "specular", "specular_power",
  };
  std::vector<std::string> series_line{
      "orientation",
      "spec",
      "x",
      "y",
  };
  std::vector<std::string> series_marginalheatmap{
      "algorithm", "gr_option_flip_x", "gr_option_flip_y", "marginalheatmap_kind", "x", "y", "z",
  };
  std::vector<std::string> series_nonuniformheatmap = series_heatmap;
  std::vector<std::string> series_nonuniformpolar_heatmap{
      "x", "y", "z", "zrange_max", "zrange_min",
  };
  std::vector<std::string> series_pie{
      "color_indices",
      "x",
  };
  std::vector<std::string> series_plot3{
      "x",
      "y",
      "z",
  };
  std::vector<std::string> series_polar{
      "r_max", "r_min", "spec", "x", "y",
  };
  std::vector<std::string> series_polar_heatmap = series_nonuniformpolar_heatmap;
  std::vector<std::string> series_polar_histogram{
      "bin_counts", "bin_edges",  "bin_width", "bin_widths",    "classes",   "draw_edges", "edge_color",
      "face_alpha", "face_color", "nbins",     "normalization", "r_max",     "r_min",      "stairs",
      "theta",      "tick",       "total",     "transparency",  "xcolormap", "ycolormap",
  };
  std::vector<std::string> series_quiver{
      "color", "u", "v", "x", "y",
  };
  std::vector<std::string> series_scatter{
      "c", "c_index", "orientation", "x", "y", "z",
  };
  std::vector<std::string> series_scatter3{
      "c",
      "x",
      "y",
      "z",
  };
  std::vector<std::string> series_shade{
      "x", "xbins", "xform", "y", "ybins",
  };
  std::vector<std::string> series_stairs{
      "orientation", "step_where", "x", "y", "z",
  };
  std::vector<std::string> series_stem{
      "orientation",
      "x",
      "y",
      "yrange_min",
  };
  std::vector<std::string> series_surface{
      "accelerate",
      "x",
      "y",
      "z",
  };
  std::vector<std::string> series_tricontour{
      "levels",
      "x",
      "y",
      "z",
  };
  std::vector<std::string> series_trisurface{
      "x",
      "y",
      "z",
  };
  std::vector<std::string> series_volume{
      "c", "c_dims", "dmax", "dmin", "x", "y", "z",
  };
  std::vector<std::string> series_wireframe{
      "x",
      "y",
      "z",
  };
  static std::map<std::string, std::vector<std::string>> element_names{
      {std::string("bar"), bar},
      {std::string("errorbar"), errorbar},
      {std::string("polar_bar"), polar_bar},
      {std::string("series_barplot"), series_barplot},
      {std::string("series_contour"), series_contour},
      {std::string("series_contourf"), series_contourf},
      {std::string("series_heatmap"), series_heatmap},
      {std::string("series_hexbin"), series_hexbin},
      {std::string("series_hist"), series_hist},
      {std::string("series_imshow"), series_imshow},
      {std::string("series_isosurface"), series_isosurface},
      {std::string("series_line"), series_line},
      {std::string("series_marginalheatmap"), series_marginalheatmap},
      {std::string("series_nonuniformheatmap"), series_nonuniformheatmap},
      {std::string("series_nonuniformpolar_heatmap"), series_nonuniformpolar_heatmap},
      {std::string("series_pie"), series_pie},
      {std::string("series_plot3"), series_plot3},
      {std::string("series_polar"), series_polar},
      {std::string("series_polar_heatmap"), series_polar_heatmap},
      {std::string("series_polar_histogram"), series_polar_histogram},
      {std::string("series_quiver"), series_quiver},
      {std::string("series_scatter"), series_scatter},
      {std::string("series_scatter3"), series_scatter3},
      {std::string("series_shade"), series_shade},
      {std::string("series_stairs"), series_stairs},
      {std::string("series_stem"), series_stem},
      {std::string("series_surface"), series_surface},
      {std::string("series_tricontour"), series_tricontour},
      {std::string("series_trisurface"), series_trisurface},
      {std::string("series_volume"), series_volume},
      {std::string("series_wireframe"), series_wireframe},
  };
  // TODO: critical update in plot means critical update inside childs

  // only do updates when there is a change made
  if (automatic_update && !starts_with(attr, "_"))
    {
      automatic_update = false;
      if (attr == "kind")
        {
          // special case for kind attributes to support switching the kind of a plot
          std::vector<std::string> line_group = {"line", "scatter"};
          std::vector<std::string> heatmap_group = {"contour",         "contourf", "heatmap",  "imshow",
                                                    "marginalheatmap", "surface",  "wireframe"};
          std::vector<std::string> isosurface_group = {"isosurface", "volume"};
          std::vector<std::string> plot3_group = {"plot3", "scatter", "scatter3", "tricont", "trisurf"};
          std::vector<std::string> barplot_group = {"barplot", "hist", "stem", "stairs"};
          std::vector<std::string> hexbin_group = {"hexbin", "shade"};
          if (std::find(line_group.begin(), line_group.end(), value) != line_group.end() &&
              std::find(line_group.begin(), line_group.end(),
                        static_cast<std::string>(element->getAttribute("kind"))) != line_group.end())
            {
              auto new_series = global_render->createSeries(static_cast<std::string>(element->getAttribute("kind")));
              element->parentElement()->insertBefore(new_series, element);
              new_series->parentElement()->setAttribute("kind",
                                                        static_cast<std::string>(element->getAttribute("kind")));
              new_series->setAttribute("x", element->getAttribute("x"));
              new_series->setAttribute("y", element->getAttribute("y"));
              new_series->setAttribute("_bbox_id", -1);
              for (const auto &child : element->children())
                {
                  if (child->localName() == "errorbars") new_series->append(child);
                  child->remove();
                }
              element->remove();
              new_series->setAttribute("_update_required", true);
              new_series->setAttribute("_delete_children", 2);
              auto legend = global_render->querySelectors("legend");
              if (legend != nullptr)
                {
                  legend->setAttribute("_delete_children", 2);
                  legend->removeAttribute("_legend_elems");
                }
            }
          else if (std::find(heatmap_group.begin(), heatmap_group.end(), value) != heatmap_group.end() &&
                   std::find(heatmap_group.begin(), heatmap_group.end(),
                             static_cast<std::string>(element->getAttribute("kind"))) != heatmap_group.end())
            {
              auto new_series = global_render->createSeries(static_cast<std::string>(element->getAttribute("kind")));
              element->parentElement()->insertBefore(new_series, element);
              new_series->parentElement()->setAttribute("kind",
                                                        static_cast<std::string>(element->getAttribute("kind")));
              new_series->setAttribute("x", element->getAttribute("x"));
              new_series->setAttribute("y", element->getAttribute("y"));
              new_series->setAttribute("_bbox_id", -1);
              if (static_cast<std::string>(element->getAttribute("kind")) == "imshow")
                {
                  new_series->setAttribute("c", element->getAttribute("z"));

                  auto context = global_render->getContext();
                  int id = static_cast<int>(global_root->getAttribute("_id"));
                  std::string str = std::to_string(id);
                  auto x = static_cast<std::string>(element->getAttribute("x"));
                  auto x_vec = GRM::get<std::vector<double>>((*context)[x]);
                  int x_length = x_vec.size();
                  auto y = static_cast<std::string>(element->getAttribute("y"));
                  auto y_vec = GRM::get<std::vector<double>>((*context)[y]);
                  int y_length = y_vec.size();
                  std::vector<int> c_dims_vec = {y_length, x_length};
                  (*context)["c_dims" + str] = c_dims_vec;
                  new_series->setAttribute("c_dims", "c_dims" + str);
                  global_root->setAttribute("_id", id++);
                }
              else if (value == "imshow")
                {
                  new_series->setAttribute("z", element->getAttribute("c"));
                }
              else
                {
                  new_series->setAttribute("z", element->getAttribute("z"));
                }
              for (const auto &child : element->children())
                {
                  child->remove();
                }
              element->remove();
              new_series->setAttribute("_update_required", true);
              new_series->setAttribute("_delete_children", 2);
            }
          else if (std::find(isosurface_group.begin(), isosurface_group.end(), value) != isosurface_group.end() &&
                   std::find(isosurface_group.begin(), isosurface_group.end(),
                             static_cast<std::string>(element->getAttribute("kind"))) != isosurface_group.end())
            {
              auto new_series = global_render->createSeries(static_cast<std::string>(element->getAttribute("kind")));
              element->parentElement()->insertBefore(new_series, element);
              new_series->parentElement()->setAttribute("kind",
                                                        static_cast<std::string>(element->getAttribute("kind")));
              new_series->setAttribute("c", element->getAttribute("c"));
              new_series->setAttribute("c_dims", element->getAttribute("c_dims"));
              new_series->setAttribute("_bbox_id", -1);
              for (const auto &child : element->children())
                {
                  child->remove();
                }
              element->remove();
              new_series->setAttribute("_update_required", true);
              new_series->setAttribute("_delete_children", 2);
            }
          else if (std::find(plot3_group.begin(), plot3_group.end(), value) != plot3_group.end() &&
                   std::find(plot3_group.begin(), plot3_group.end(),
                             static_cast<std::string>(element->getAttribute("kind"))) != plot3_group.end())
            {
              auto new_series = global_render->createSeries(static_cast<std::string>(element->getAttribute("kind")));
              element->parentElement()->insertBefore(new_series, element);
              new_series->parentElement()->setAttribute("kind",
                                                        static_cast<std::string>(element->getAttribute("kind")));
              new_series->setAttribute("x", element->getAttribute("x"));
              new_series->setAttribute("y", element->getAttribute("y"));
              new_series->setAttribute("z", element->getAttribute("z"));
              new_series->setAttribute("_bbox_id", -1);
              for (const auto &child : element->children())
                {
                  child->remove();
                }
              element->remove();
              new_series->setAttribute("_update_required", true);
              new_series->setAttribute("_delete_children", 2);
            }
          else if (std::find(barplot_group.begin(), barplot_group.end(), value) != barplot_group.end() &&
                   std::find(barplot_group.begin(), barplot_group.end(),
                             static_cast<std::string>(element->getAttribute("kind"))) != barplot_group.end())
            {
              auto new_series = global_render->createSeries(static_cast<std::string>(element->getAttribute("kind")));
              element->parentElement()->insertBefore(new_series, element);
              new_series->parentElement()->setAttribute("kind",
                                                        static_cast<std::string>(element->getAttribute("kind")));
              new_series->setAttribute("x", element->getAttribute("x"));
              new_series->setAttribute("_bbox_id", -1);
              if (static_cast<std::string>(element->getAttribute("kind")) == "barplot" ||
                  static_cast<std::string>(element->getAttribute("kind")) == "stem")
                {
                  if (value == "hist")
                    {
                      new_series->setAttribute("y", element->getAttribute("weights"));
                    }
                  else if (value == "stairs")
                    {
                      new_series->setAttribute("y", element->getAttribute("z"));
                    }
                  else
                    {
                      new_series->setAttribute("y", element->getAttribute("y"));
                    }
                }
              else if (static_cast<std::string>(element->getAttribute("kind")) == "hist")
                {
                  if (value == "stairs")
                    {
                      new_series->setAttribute("weights", element->getAttribute("z"));
                    }
                  else
                    {
                      new_series->setAttribute("weights", element->getAttribute("y"));
                    }
                }
              else
                {
                  if (value == "hist")
                    {
                      new_series->setAttribute("z", element->getAttribute("weights"));
                    }
                  else
                    {
                      new_series->setAttribute("z", element->getAttribute("y"));
                    }
                }

              for (const auto &child : element->children())
                {
                  if (child->localName() == "errorbars") new_series->append(child);
                  child->remove();
                }
              element->remove();
              new_series->setAttribute("_update_required", true);
              new_series->setAttribute("_delete_children", 2);
            }
          else if (std::find(hexbin_group.begin(), hexbin_group.end(), value) != hexbin_group.end() &&
                   std::find(hexbin_group.begin(), hexbin_group.end(),
                             static_cast<std::string>(element->getAttribute("kind"))) != hexbin_group.end())
            {
              auto new_series = global_render->createSeries(static_cast<std::string>(element->getAttribute("kind")));
              element->parentElement()->insertBefore(new_series, element);
              new_series->parentElement()->setAttribute("kind",
                                                        static_cast<std::string>(element->getAttribute("kind")));
              new_series->setAttribute("x", element->getAttribute("x"));
              new_series->setAttribute("y", element->getAttribute("y"));
              new_series->setAttribute("_bbox_id", -1);
              for (const auto &child : element->children())
                {
                  child->remove();
                }
              element->remove();
              new_series->setAttribute("_update_required", true);
              new_series->setAttribute("_delete_children", 2);
            }
          else
            {
              element->setAttribute("kind", value);
              fprintf(stderr, "Update kind %s to %s is not possible\n",
                      static_cast<std::string>(element->getAttribute("kind")).c_str(), value.c_str());
              std::cerr << toXML(element->getRootNode(), GRM::SerializerOptions{std::string(2, ' ')}) << "\n";
            }
        }
      else
        {
          if (auto search = element_names.find(element->localName()); search != element_names.end())
            {
              // attributes which causes a critical update
              if (std::find(search->second.begin(), search->second.end(), attr) != search->second.end())
                {
                  element->setAttribute("_update_required", true);
                  if (attr != "orientation") element->setAttribute("_delete_children", 2);
                  if (attr == "orientation")
                    {
                      resetOldBoundingBoxes(element->parentElement());
                      resetOldBoundingBoxes(element);
                      element->removeAttribute("_bbox_id");
                    }
                  if (element->localName() == "polar_bar") element->setAttribute("_delete_children", 1);
                  if (element->localName() == "series_polar_histogram" && (attr == "nbins" || attr == "bin_widths"))
                    {
                      for (const auto &child : element->children())
                        {
                          if (child->localName() == "polar_bar") child->setAttribute("_update_required", true);
                        }
                    }
                }
            }
          else if (starts_with(element->localName(), "series"))
            {
              // conditional critical attributes
              auto kind = static_cast<std::string>(element->getAttribute("kind"));
              if (kind == "heatmap" || kind == "polar_heatmap")
                {
                  if (element->hasAttribute("crange_max") && element->hasAttribute("crange_min"))
                    {
                      if (attr == "crange_max" || attr == "crange_min")
                        {
                          element->setAttribute("_update_required", true);
                          element->setAttribute("_delete_children", 2);
                        }
                    }
                }
              if (kind == "heatmap" || kind == "surface" || kind == "polar_heatmap")
                {
                  if (!element->hasAttribute("x") && !element->hasAttribute("y"))
                    {
                      if (attr == "zdims_max" || attr == "zdims_min")
                        {
                          element->setAttribute("_update_required", true);
                          element->setAttribute("_delete_children", 2);
                        }
                    }
                  if (!element->hasAttribute("x"))
                    {
                      if (attr == "xrange_max" || attr == "xrange_min")
                        {
                          element->setAttribute("_update_required", true);
                          element->setAttribute("_delete_children", 2);
                        }
                    }
                  if (!element->hasAttribute("y"))
                    {
                      if (attr == "yrange_max" || attr == "yrange_min")
                        {
                          element->setAttribute("_update_required", true);
                          element->setAttribute("_delete_children", 2);
                        }
                    }
                }
              else if (kind == "marginalheatmap" && (attr == "xind" || attr == "yind"))
                {
                  auto xind = static_cast<int>(element->getAttribute("xind"));
                  auto yind = static_cast<int>(element->getAttribute("yind"));
                  if (attr == "xind" && yind != -1) element->setAttribute("_update_required", true);
                  if (attr == "yind" && xind != -1) element->setAttribute("_update_required", true);
                }
            }
          else if ((attr == "size_x" || attr == "size_y") && element->localName() == "figure" &&
                   static_cast<int>(element->getAttribute("active")))
            {
              // imshow needs vp which gets inflicted by the size attributes in figure, this means when the size is
              // changed the imshow_series must be recalculated
              auto imshow = global_render->querySelectorsAll("series_imshow");
              for (const auto &imshow_elem : imshow)
                {
                  imshow_elem->setAttribute("_update_required", true);
                }

              // reset the bounding boxes for all series
              for (const auto &child : element->children()) // plot level
                {
                  for (const auto &childchild : child->children())
                    {
                      if (starts_with(childchild->localName(), "series_")) resetOldBoundingBoxes(childchild);
                    }
                }

              // reset the bounding boxes for elements in list
              for (const std::string name : {"bar", "pie_segment", "colorbar", "polar_bar", "coordinate_system",
                                             "axes_text_group", "plot", "errorbar"})
                {
                  for (const auto &elem : element->querySelectorsAll(name))
                    {
                      resetOldBoundingBoxes(elem);
                      // plot gets calculated to quit so this special case is needed to get the right bboxes
                      if (name == "plot") elem->removeAttribute("_bbox_id");
                    }
                }

              // reset the bounding boxes for figure
              resetOldBoundingBoxes(element);
              element->removeAttribute("_bbox_id");
            }
          else if (attr == "reset_ranges" && element->localName() == "plot")
            {
              // when the ranges gets reseted the bounding boxes of the series can be wrong, to solve this problem
              // they get calculated again out of their children
              for (const auto &child : element->children())
                {
                  if (starts_with(child->localName(), "series_")) resetOldBoundingBoxes(child);
                }

              // reset the bounding boxes for elements in list
              for (const std::string name :
                   {"bar", "pie_segment", "colorbar", "polar_bar", "axes_text_group", "errorbar"})
                {
                  for (const auto &elem : element->querySelectorsAll(name))
                    {
                      resetOldBoundingBoxes(elem);
                    }
                }
            }
        }
      global_root->setAttribute("_modified", true);

      automatic_update = true;
    }
}

void renderCaller()
{
  if (global_root && static_cast<int>(global_root->getAttribute("_modified")) && automatic_update)
    {
      global_render->process_tree();
    }
}

void GRM::Render::setActiveFigure(const std::shared_ptr<GRM::Element> &element)
{
  auto result = this->firstChildElement()->querySelectorsAll("[active=1]");
  for (auto &child : result)
    {
      child->setAttribute("active", 0);
    }
  element->setAttribute("active", 1);
}

std::shared_ptr<GRM::Element> GRM::Render::getActiveFigure()
{
  return active_figure;
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~ modify context ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 */

void updateContextAttribute(const std::shared_ptr<GRM::Element> &element, const std::string &attr,
                            const GRM::Value &old_value)
{
  // when the attribute is a context the counter for the attribute value inside the context must be modified
  if (valid_context_keys.find(attr) != valid_context_keys.end())
    {
      auto new_value = element->getAttribute(attr);
      if (new_value.isString())
        {
          auto context = global_render->getContext();
          (*context)[attr].use_context_key(static_cast<std::string>(new_value), static_cast<std::string>(old_value));
        }
    }
}

void deleteContextAttribute(const std::shared_ptr<GRM::Element> &element)
{
  auto elem_attribs = element->getAttributeNames();
  std::vector<std::string> attr_inter;
  std::vector<std::string> elem_attribs_vec(elem_attribs.begin(), elem_attribs.end());
  std::vector<std::string> valid_keys_copy(valid_context_keys.begin(), valid_context_keys.end());

  std::sort(elem_attribs_vec.begin(), elem_attribs_vec.end());
  std::sort(valid_keys_copy.begin(), valid_keys_copy.end());
  std::set_intersection(elem_attribs_vec.begin(), elem_attribs_vec.end(), valid_keys_copy.begin(),
                        valid_keys_copy.end(), std::back_inserter(attr_inter));

  auto context = global_render->getContext();
  for (const auto &attr : attr_inter)
    {
      auto value = element->getAttribute(attr);
      if (value.isString())
        {
          (*context)[attr].decrement_key(static_cast<std::string>(value));
        }
    }
}
