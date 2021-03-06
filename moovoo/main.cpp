///////////////////////////////////////////////////////////////////////////////
//
// (C) Andy Thomason 2017
//
// Moovoo: a Vulkan molecule explorer
//

#include <Python.h>

#include <vku/vku_framework.hpp>

#include <glm/glm.hpp>

//#define GLM_FORCE_DEPTH_ZERO_TO_ONE
//#define GLM_FORCE_LEFT_HANDED
#include <glm/ext.hpp>

#include <gilgamesh/mesh.hpp>
#include <gilgamesh/distance_field.hpp>
#include <gilgamesh/decoders/pdb_decoder.hpp>
#include <vector>
#include <boost/python.hpp>

#define STB_TRUETYPE_IMPLEMENTATION
#include <stb/stb_truetype.h>

#ifdef WIN32
#define FOUNT_NAME "C:/windows/fonts/arial.ttf"
#else
#define FOUNT_NAME "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf"
#endif

namespace moovoo {

namespace bp = boost::python;

using mat4 = glm::mat4;
using vec2 = glm::vec2;
using vec3 = glm::vec3;
using vec4 = glm::vec4;
using uint = uint32_t;


struct Atom {
  vec3 pos;
  float radius;
  vec3 colour;
  float mass;
  vec3 prevPos;
  int selected;
  vec3 acc;
  int connections[5];
};

struct Connection {
  uint from;
  uint to;
  float naturalLength;
  float springConstant;
};
  
struct Pick {
  static constexpr uint fifoSize = 4;
  uint atom;
  uint distance;
};

struct Instance {
  mat4 modelToWorld;
};
  
struct PushConstants {
  mat4 worldToPerspective;
  mat4 modelToWorld;
  mat4 cameraToWorld;

  vec3 rayStart;
  float timeStep;
  vec3 rayDir;
  uint numAtoms;
  uint numConnections;
  uint pickIndex;
  uint pass;
};

struct Glyph {
  vec2 uv0;
  vec2 uv1;
  vec2 pos0;
  vec2 pos1;
  vec3 colour;
  int pad2;
  vec3 origin;
  int pad;
};

class StandardLayout {
public:
  StandardLayout() {
  }

  StandardLayout(vk::Device device) {
    vku::DescriptorSetLayoutMaker dslm{};
    dslm.buffer(0U, vk::DescriptorType::eStorageBuffer, vk::ShaderStageFlagBits::eAll, 1); // Atoms
    dslm.buffer(1U, vk::DescriptorType::eStorageBuffer, vk::ShaderStageFlagBits::eAll, 1); // Fount glyphs
    dslm.buffer(2U, vk::DescriptorType::eStorageBuffer, vk::ShaderStageFlagBits::eCompute, 1); // Pick
    dslm.buffer(3U, vk::DescriptorType::eStorageBuffer, vk::ShaderStageFlagBits::eAll, 1); // Connections
    dslm.buffer(4U, vk::DescriptorType::eCombinedImageSampler, vk::ShaderStageFlagBits::eAll, 1); // Cube map
    dslm.buffer(5U, vk::DescriptorType::eCombinedImageSampler, vk::ShaderStageFlagBits::eAll, 1); // Fount map
    dslm.buffer(6U, vk::DescriptorType::eStorageBuffer, vk::ShaderStageFlagBits::eAll, 1); // Instances
    dslm.buffer(7U, vk::DescriptorType::eStorageBuffer, vk::ShaderStageFlagBits::eAll, 1); // Solvent Acessible
    layout_ = dslm.createUnique(device);

    vku::PipelineLayoutMaker plm{};
    plm.descriptorSetLayout(*layout_);
    plm.pushConstantRange(vk::ShaderStageFlagBits::eAll, 0, sizeof(PushConstants));
    pipelineLayout_ = plm.createUnique(device);
  }

  vk::PipelineLayout pipelineLayout() const { return *pipelineLayout_; }
  vk::DescriptorSetLayout descriptorSetLayout() const { return *layout_; }
private:
  vk::UniqueDescriptorSetLayout layout_;
  vk::UniquePipelineLayout pipelineLayout_;
};

class GraphicsPipeline {
public:
  GraphicsPipeline() {
  }

  GraphicsPipeline(
    vk::Device device, vk::PipelineCache cache, vk::RenderPass renderPass, uint32_t width, uint32_t height, vk::PipelineLayout pipelineLayout,
    const std::string &vertshader, const std::string &fragshader
  ) {
    vert_ = vku::ShaderModule{device, vertshader};
    frag_ = vku::ShaderModule{device, fragshader};

    vku::PipelineMaker pm{width, height};
    pm.shader(vk::ShaderStageFlagBits::eVertex, vert_);
    pm.shader(vk::ShaderStageFlagBits::eFragment, frag_);
    pm.depthTestEnable(VK_TRUE);

    pipeline_ = pm.createUnique(device, cache, pipelineLayout, renderPass);
  }

  GraphicsPipeline(
    vk::Device device, vk::PipelineCache cache, vk::RenderPass renderPass, vk::PipelineLayout pipelineLayout,
    const std::string &vertshader, const std::string &fragshader,
    vku::PipelineMaker pm
  ) {
    vert_ = vku::ShaderModule{device, vertshader};
    frag_ = vku::ShaderModule{device, fragshader};

    pm.shader(vk::ShaderStageFlagBits::eVertex, vert_);
    pm.shader(vk::ShaderStageFlagBits::eFragment, frag_);

    pipeline_ = pm.createUnique(device, cache, pipelineLayout, renderPass);
  }

  vk::Pipeline pipeline() const { return *pipeline_; }
private:
  vk::UniquePipeline pipeline_;
  vku::ShaderModule vert_;
  vku::ShaderModule frag_;
};

class FountPipeline {
public:
  FountPipeline() {
  }

  FountPipeline(vk::Device device, vk::PipelineCache cache, vk::RenderPass renderPass, uint32_t width, uint32_t height, vk::PipelineLayout pipelineLayout) {
    vert_ = vku::ShaderModule{device, BINARY_DIR "fount.vert.spv"};
    frag_ = vku::ShaderModule{device, BINARY_DIR "fount.frag.spv"};

    vku::PipelineMaker pm{width, height};
    pm.shader(vk::ShaderStageFlagBits::eVertex, vert_);
    pm.shader(vk::ShaderStageFlagBits::eFragment, frag_);

    // blend with premultiplied alpha
    pm.blendBegin(1);
    pm.blendSrcColorBlendFactor(vk::BlendFactor::eOne);

    pipeline_ = pm.createUnique(device, cache, pipelineLayout, renderPass);
  }

  vk::Pipeline pipeline() const { return *pipeline_; }
private:
  vk::UniquePipeline pipeline_;
  vku::ShaderModule vert_;
  vku::ShaderModule frag_;
};

class DynamicsPipeline {
public:
  DynamicsPipeline() {
  }

  DynamicsPipeline(vk::Device device, vk::PipelineCache cache, vk::RenderPass renderPass, uint32_t width, uint32_t height, vk::PipelineLayout pipelineLayout) {
    comp_ = vku::ShaderModule{device, BINARY_DIR "dynamics.comp.spv"};

    vku::ComputePipelineMaker cpm{};
    cpm.shader(vk::ShaderStageFlagBits::eCompute, comp_);
    pipeline_ = cpm.createUnique(device, cache, pipelineLayout);
  }

  vk::Pipeline pipeline() const { return *pipeline_; }
private:
  vk::UniquePipeline pipeline_;
  vku::ShaderModule comp_;
};

class TextModel {
public:
  TextModel() {
    numGlyphs_ = 0;
  }

  TextModel(const std::string &filename, vk::Device device, vk::PhysicalDeviceMemoryProperties memprops, vk::CommandPool commandPool, vk::Queue queue) {
    using buf = vk::BufferUsageFlagBits;
    maxGlyphs_ = 8192;
    numGlyphs_ = 0;
    glyphs_ = vku::GenericBuffer(device, memprops, buf::eStorageBuffer|buf::eTransferDst, maxGlyphs_ * sizeof(Glyph), vk::MemoryPropertyFlagBits::eHostVisible);

    uint32_t fountWidth = 1024;
    uint32_t fountHeight = 1024;
    fountMap_ = vku::TextureImage2D{device, memprops, fountWidth, fountHeight, 1, vk::Format::eR8Unorm};
    std::vector<uint8_t> fountBytes(fountWidth * fountHeight);
    auto fountData = vku::loadFile(filename);

    int charCount = '~'-' '+1;
    charInfo_ = std::vector<stbtt_packedchar>(charCount);

    stbtt_pack_context context;
    int oversampleX = 4;
    int oversampleY = 4;
    float fountSize = 40;
    stbtt_PackBegin(&context, fountBytes.data(), fountWidth, fountHeight, 0, 1, nullptr);
    stbtt_PackSetOversampling(&context, oversampleX, oversampleY);
    stbtt_PackFontRange(&context, fountData.data(), 0, fountSize, ' ', charCount, charInfo_.data());
    stbtt_PackEnd(&context);

    fountMap_.upload(device, fountBytes, commandPool, memprops, queue);

    vku::SamplerMaker fsm{};
    fsm.magFilter(vk::Filter::eNearest);
    fsm.minFilter(vk::Filter::eNearest);
    fsm.mipmapMode(vk::SamplerMipmapMode::eNearest);
    fountSampler_ = fsm.createUnique(device);

    pGlyphs_ = (Glyph *)glyphs_.map(device);
  }

  void reset() {
    numGlyphs_ = 0;
  }

  glm::vec2 draw(glm::vec3 origin, glm::vec2 pos, glm::vec3 colour, glm::vec2 scale, const std::string &text) {
    stbtt_aligned_quad quad{};
    numGlyphs_ = 0;
    for (auto c : text) {
      if (c < ' ' || c > '~') continue;
      if (numGlyphs_ == maxGlyphs_) break;
      auto &g = pGlyphs_[numGlyphs_++];
      vec2 offset;
      stbtt_GetPackedQuad(charInfo_.data(), fountMap_.extent().width, fountMap_.extent().height, c - ' ', &offset.x, &offset.y, &quad, 1);
      //printf("%f %f %f %f %f %f\n", offset.x, offset.y, quad.x0, quad.y0, quad.x1, quad.y1);
      g.colour = colour;
      g.pos0 = pos + vec2(quad.x0, -quad.y0) * scale;
      g.pos1 = pos + vec2(quad.x1, -quad.y1) * scale;
      g.uv0 = vec2(quad.s0, quad.t0);
      g.uv1 = vec2(quad.s1, quad.t1);
      g.origin = origin;
      pos += offset * scale;
    }
    return pos;
  }

  vk::Buffer glyphs() const { return glyphs_.buffer(); }
  vk::ImageView imageView() const { return fountMap_.imageView(); }
  vk::Sampler sampler() const { return *fountSampler_; }
  int maxGlyphs() const { return maxGlyphs_; }
  int numGlyphs() const { return numGlyphs_; }
private:
  vku::GenericBuffer glyphs_;
  vku::TextureImage2D fountMap_;
  vk::UniqueSampler fountSampler_;
  Glyph *pGlyphs_;
  int maxGlyphs_;
  int numGlyphs_;
  std::vector<stbtt_packedchar> charInfo_;
};

class View;

class Context {
public:
  Context() {
    // Initialise the GLFW framework.
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

    fw_ = vku::Framework{"moovoo"};
    if (!fw_.ok()) {
      throw std::runtime_error("Vulkan framework creation failed");
    }

    typedef vk::CommandPoolCreateFlagBits ccbits;

    vk::CommandPoolCreateInfo cpci{ ccbits::eTransient|ccbits::eResetCommandBuffer, fw_.graphicsQueueFamilyIndex() };
    commandPool_ = fw_.device().createCommandPoolUnique(cpci);
  }

  Context(const Context &rhs) {}

  ~Context() {
    //for (auto cm : cm_) cm->kill();
    printf("~Context\n");
  }

  // Forward declared
  void mainloop();

  vk::Instance instance() const { return fw_.instance(); }
  const vk::PhysicalDeviceMemoryProperties &memprops() { return fw_.memprops(); }
  vk::Queue queue() const { return fw_.graphicsQueue(); }
  vk::PhysicalDevice physicalDevice() const { return fw_.physicalDevice(); }
  const vk::CommandPool &commandPool() { return *commandPool_; }
  vk::Device device() { return fw_.device(); }
  uint32_t graphicsQueueFamilyIndex() { return fw_.graphicsQueueFamilyIndex(); }
  vk::PipelineCache pipelineCache() { return fw_.pipelineCache(); }
  vk::DescriptorPool descriptorPool() { return fw_.descriptorPool(); }
  void addView(View *view) { views_.push_back(view); }
private:
  vku::Framework fw_;
  vk::Device device_;
  vk::UniqueCommandPool commandPool_;
  std::vector<View*> views_;
};

class Model {
public:
  Model() {}

  Model(Context &inst, boost::python::object bytes) {
    Py_buffer pybuf;
    if (PyObject_GetBuffer(bytes.ptr(), &pybuf, PyBUF_SIMPLE) < 0) {
      throw std::runtime_error("Model expects buffer object");
    }

    const uint8_t *b = (const uint8_t *)pybuf.buf;
    const uint8_t *e = b + pybuf.len;
    pdb_ = gilgamesh::pdb_decoder(b, e);
    PyBuffer_Release(&pybuf);

    std::string chains = pdb_.chains();
    pdbAtoms_ = pdb_.atoms(chains);

    glm::vec3 mean(0);
    glm::vec3 min(1e38f);
    glm::vec3 max(-1e38f);
    for (auto &atom : pdbAtoms_) {
      glm::vec3 pos = atom.pos();
      min = glm::min(min, pos);
      max = glm::max(max, pos);
      mean += pos;
    }
    mean /= (float)pdbAtoms_.size();

    std::vector<Atom> atoms;
    std::vector<glm::vec3> pos;
    std::vector<float> radii;
    for (auto &atom : pdbAtoms_) {
      glm::vec3 colour = atom.colorByElement();
      colour.r = colour.r * 0.75f + 0.25f;
      colour.g = colour.g * 0.75f + 0.25f;
      colour.b = colour.b * 0.75f + 0.25f;
      Atom a{};
      a.pos = a.prevPos = atom.pos() - mean;
      pos.push_back(a.pos);
      float scale = 0.1f;
      if (atom.atomNameIs("N") || atom.atomNameIs("CA") || atom.atomNameIs("C") || atom.atomNameIs("P")) scale = 0.4f;
      float radius = atom.vanDerVaalsRadius();
      radii.push_back(radius);
      a.radius = radius * scale;
      a.colour = colour;
      a.acc = glm::vec3(0, 0, 0);
      a.mass = 1.0f;
      std::fill(std::begin(a.connections), std::end(a.connections), -1);
      atoms.push_back(a);
    }

    min -= mean;
    max -= mean;
    glm::vec3 extent = max - min;
    float grid_spacing = 1.0f;
    int xdim = int(extent.x / grid_spacing) + 1;
    int ydim = int(extent.y / grid_spacing) + 1;
    int zdim = int(extent.z / grid_spacing) + 1;

    printf("%dx%dx%d\n", xdim, ydim, zdim);
    gilgamesh::distance_field df(xdim, ydim, zdim, grid_spacing, min, pos, radii);

    auto &distance = df.distances();

    auto idx = [xdim, ydim, zdim](int x, int y, int z) { return (z * ydim + y) + xdim + x; };

    std::vector<glm::vec3> solventAcessible;
    for (int z = 0; z != zdim; ++z) {
      for (int y = 0; y != ydim; ++y) {
        for (int x = 0; x != xdim; ++x) {
          int i = idx(x, y, z);
          float d000 = distance[i];
          float d100 = distance[i + idx(1, 0, 0)];
          float d010 = distance[i + idx(0, 1, 0)];
          float d001 = distance[i + idx(0, 0, 1)];
          glm::vec3 pos = min + glm::vec3(x, y, z) * grid_spacing;
          if (d000 * d100 < 0) {
            float d = grid_spacing + d000 / (d000 - d100);
            solventAcessible.push_back(glm::vec3(pos.x + d, pos.y, pos.z));
          }
          if (d000 * d010 < 0) {
            float d = grid_spacing + d000 / (d000 - d010);
            solventAcessible.push_back(glm::vec3(pos.x, pos.y + d, pos.z));
          }
          if (d000 * d001 < 0) {
            float d = grid_spacing + d000 / (d000 - d001);
            solventAcessible.push_back(glm::vec3(pos.x, pos.y, pos.z + d));
          }
        }
      }
    }
    numSolventAcessible_ = (uint32_t)solventAcessible.size();

    std::vector<std::pair<int, int>> pairs;
    int prevC = -1;
    char prevChainID = '?';
    for (size_t bidx = 0; bidx != pdbAtoms_.size(); ) {
      // At the start of every Amino Acid, connect the atoms.
      char chainID = pdbAtoms_[bidx].chainID();
      char iCode = pdbAtoms_[bidx].iCode();
      size_t eidx = pdb_.nextResidue(pdbAtoms_, bidx);
      if (prevChainID != chainID) prevC = -1;

      // iCode is 'A' etc. for alternates.
      if (iCode == ' ' || iCode == '?') {
        /*for (size_t i = bidx; i != eidx; ++i) {
          for (size_t j = i+1; j <= eidx; ++j) {
            if (length(atoms[i].pos - atoms[j].pos) < vdv[i] + vdv[j]) {
              pairs.emplace_back((int)i, (int)j);
            }
          }
        }*/
        prevC = pdb_.addImplicitConnections(pdbAtoms_, pairs, bidx, eidx, prevC, false);
        prevChainID = chainID;
      }
      bidx = eidx;
    }

    for (auto &p : pairs) {
      Atom &from = atoms[p.first];
      Atom &to = atoms[p.second];
      for (auto &i : from.connections) {
        if (i == -1) {
          i = p.second;
          break;
        }
      }
      for (auto &i : to.connections) {
        if (i == -1) {
          i = p.first;
          break;
        }
      }
    }

    std::vector<Connection> conns;
    for (auto &p : pairs) {
      Connection c;
      c.from = p.first;
      c.to = p.second;
      glm::vec3 p1 = atoms[c.from].pos;
      glm::vec3 p2 = atoms[c.to].pos;
      c.naturalLength = glm::length(p2 - p1);
      c.springConstant = 100;
      conns.push_back(c);
    }

    if (0) {
      atoms.resize(0);
      conns.resize(0);

      Atom a{};
      a.colour = glm::vec3(1);
      a.mass = 1;
      a.pos = glm::vec3(-2, 0, 0);
      a.radius = 1;
      atoms.push_back(a);
      a.pos = glm::vec3( 0, 0, 0);
      atoms.push_back(a);
      a.pos = glm::vec3( 2, 0, 0);
      atoms.push_back(a);

      Connection c{};
      c.from = 0;
      c.to = 1;
      c.naturalLength = 2;
      c.springConstant = 10;
      conns.push_back(c);
      c.from = 1;
      c.to = 2;
      conns.push_back(c);
    }

    std::vector<Instance> instances;
    for (auto &mat : pdb_.instanceMatrices()) {
      Instance ins{};
      ins.modelToWorld = mat;
      instances.push_back(ins);
    }

    if (instances.empty()) {
      Instance ins{};
      instances.push_back(ins);
    }

    numAtoms_ = (uint32_t)atoms.size();
    numConnections_ = (uint32_t)conns.size();
    numContexts_ = (uint32_t)instances.size();

    auto memprops = inst.memprops();
    auto device = inst.device();
    auto commandPool = inst.commandPool();
    auto queue = inst.queue();

    using buf = vk::BufferUsageFlagBits;
    atoms_ = vku::GenericBuffer(device, memprops, buf::eStorageBuffer|buf::eTransferDst, (numAtoms_+1) * sizeof(Atom), vk::MemoryPropertyFlagBits::eHostVisible);
    pick_ = vku::GenericBuffer(device, memprops, buf::eStorageBuffer, sizeof(Pick) * Pick::fifoSize, vk::MemoryPropertyFlagBits::eHostVisible);
    conns_ = vku::GenericBuffer(device, memprops, buf::eStorageBuffer|buf::eTransferDst, sizeof(Connection) * (numConnections_+1), vk::MemoryPropertyFlagBits::eHostVisible);
    instances_ = vku::GenericBuffer(device, memprops, buf::eStorageBuffer|buf::eTransferDst, sizeof(Context) * numContexts_, vk::MemoryPropertyFlagBits::eHostVisible);
    solventAcessible_ = vku::GenericBuffer(device, memprops, buf::eStorageBuffer|buf::eTransferDst, sizeof(glm::vec3) * (solventAcessible.size()+1), vk::MemoryPropertyFlagBits::eHostVisible);

    atoms_.upload(device, memprops, commandPool, queue, atoms);
    conns_.upload(device, memprops, commandPool, queue, conns);
    instances_.upload(device, memprops, commandPool, queue, instances);
    solventAcessible_.upload(device, memprops, commandPool, queue, solventAcessible);
    pAtoms_ = (Atom*)atoms_.map(device);

    printf("done\n");
  }

  void updateDescriptorSet(vk::Device device, vk::DescriptorSetLayout layout, vk::DescriptorPool descriptorPool, vk::Sampler cubeSampler, vk::ImageView cubeImageView, vk::Sampler fountSampler, vk::ImageView fountImageView, vk::Buffer glyphs, int maxGlyphs) {
    vku::DescriptorSetMaker dsm{};
    dsm.layout(layout);
    auto StandardLayout = dsm.create(device, descriptorPool);
    descriptorSet_ = StandardLayout[0];

    vku::DescriptorSetUpdater update;
    update.beginDescriptorSet(descriptorSet_);

    // Point the descriptor set at the storage buffer.
    update.beginBuffers(0, 0, vk::DescriptorType::eStorageBuffer);
    update.buffer(atoms_.buffer(), 0, numAtoms_ * sizeof(Atom));
    update.beginBuffers(1, 0, vk::DescriptorType::eStorageBuffer);
    update.buffer(glyphs, 0, maxGlyphs * sizeof(Glyph));
    update.beginBuffers(2, 0, vk::DescriptorType::eStorageBuffer);
    update.buffer(pick_.buffer(), 0, sizeof(Pick) * Pick::fifoSize);
    update.beginBuffers(3, 0, vk::DescriptorType::eStorageBuffer);
    update.buffer(conns_.buffer(), 0, sizeof(Connection) * numConnections_);
    update.beginImages(4, 0, vk::DescriptorType::eCombinedImageSampler);
    update.image(cubeSampler, cubeImageView, vk::ImageLayout::eShaderReadOnlyOptimal);
    update.beginImages(5, 0, vk::DescriptorType::eCombinedImageSampler);
    update.image(fountSampler, fountImageView, vk::ImageLayout::eShaderReadOnlyOptimal);
    update.beginBuffers(6, 0, vk::DescriptorType::eStorageBuffer);
    update.buffer(instances_.buffer(), 0, numContexts_ * sizeof(Context));
    update.beginBuffers(7, 0, vk::DescriptorType::eStorageBuffer);
    update.buffer(solventAcessible_.buffer(), 0, solventAcessible_.size());

    update.update(device);
  }

  vk::DescriptorSet descriptorSet() const { return descriptorSet_; }
  uint32_t numAtoms() const { return numAtoms_; }
  uint32_t numConnections() const { return numConnections_; }
  uint32_t numContexts() const { return numContexts_; }
  uint32_t numSolventAcessible() const { return numSolventAcessible_; }
  const vku::GenericBuffer &atoms() const { return atoms_; }
  Atom *pAtoms() const { return pAtoms_; }
  const vku::GenericBuffer &pick() const { return pick_; }
  const vku::GenericBuffer &conns() const { return conns_; }
  const vku::GenericBuffer &solventAcessible() const { return solventAcessible_; }
  const std::vector<gilgamesh::pdb_decoder::atom> &pdbAtoms() { return pdbAtoms_; }

  Model(const Model &rhs) {}
  void operator=(const Model &rhs) {}

  Model(Model &&rhs) = default;
  Model &operator=(Model &&rhs) = default;

private:
  uint32_t numAtoms_;
  uint32_t numConnections_;
  uint32_t numContexts_;
  uint32_t numSolventAcessible_;
  vku::GenericBuffer atoms_;
  vku::GenericBuffer pick_;
  vku::GenericBuffer conns_;
  vku::GenericBuffer instances_;
  vku::GenericBuffer solventAcessible_;
  vk::DescriptorSet descriptorSet_;
  gilgamesh::pdb_decoder pdb_;
  std::vector<uint8_t> pdb_text_;
  std::vector<gilgamesh::pdb_decoder::atom> pdbAtoms_;
  Atom *pAtoms_;
};

/// One person's view of the world.
class View {
public:
  View() : model_(*(Model*)nullptr) {}

  View(Context &ctxt, const std::string &mode, bp::object &pyModel, const bp::object &size) : model_(bp::extract<Model&>(pyModel)) {
    uint32_t width = bp::extract<int>(size[0]);
    uint32_t height = bp::extract<int>(size[1]);
    auto instance = ctxt.instance();
    auto device = ctxt.device();
    auto memprops = ctxt.memprops();
    auto graphicsQueueFamilyIndex = ctxt.graphicsQueueFamilyIndex();
    auto queue = ctxt.queue();
    auto physicalDevice = ctxt.physicalDevice();
    ctxt.addView(this);
    width_ = width;
    height_ = height;

    if (mode == "Window") {
      // Make a window
      const char *title = "moovoo";
      bool fullScreen = false;
      GLFWmonitor *monitor = nullptr;
      glfwwindow_ = glfwCreateWindow(width, height, title, monitor, nullptr);
      window_ = vku::Window(instance, device, physicalDevice, graphicsQueueFamilyIndex, glfwwindow_);
      renderPass_ = window_.renderPass();
    }

    depthStencilImage_ = vku::DepthStencilImage(device, memprops, width, height);
    colorAttachmentImage_ = vku::ColorAttachmentImage(device, memprops, width, height);

    using buf = vk::BufferUsageFlagBits;
    transferBuffer_ = vku::GenericBuffer(device, memprops, buf::eTransferDst, width*height*4, vk::MemoryPropertyFlagBits::eHostVisible);
    mappedTransferBuffer_ = transferBuffer_.map(device);

    typedef vk::CommandPoolCreateFlagBits ccbits;
    vk::CommandPoolCreateInfo cpci{ ccbits::eTransient|ccbits::eResetCommandBuffer, ctxt.graphicsQueueFamilyIndex() };
    commandPool_ = device.createCommandPoolUnique(cpci);


    // Build the renderpass using two attachments, colour and depth/stencil.
    /*vku::RenderpassMaker rpm;

    // The only colour attachment.
    rpm.attachmentBegin(vk::Format::eB8G8R8A8Unorm);
    rpm.attachmentLoadOp(vk::AttachmentLoadOp::eClear);
    rpm.attachmentStoreOp(vk::AttachmentStoreOp::eStore);
    rpm.attachmentFinalLayout(vk::ImageLayout::eTransferSrcOptimal);

    // The depth/stencil attachment.
    rpm.attachmentBegin(depthStencilImage_.format());
    rpm.attachmentLoadOp(vk::AttachmentLoadOp::eClear);
    rpm.attachmentFinalLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal);

    // A subpass to render using the above two attachments.
    rpm.subpassBegin(vk::PipelineBindPoint::eGraphics);
    rpm.subpassColorAttachment(vk::ImageLayout::eColorAttachmentOptimal, 0);
    rpm.subpassDepthStencilAttachment(vk::ImageLayout::eDepthStencilAttachmentOptimal, 1);

    // A dependency to reset the layout of both attachments.
    rpm.dependencyBegin(VK_SUBPASS_EXTERNAL, 0);
    rpm.dependencySrcStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput);
    rpm.dependencyDstStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput);
    rpm.dependencyDstAccessMask(vk::AccessFlagBits::eColorAttachmentRead|vk::AccessFlagBits::eColorAttachmentWrite);

    // Use the maker object to construct the vulkan object
    renderPass_ = rpm.createUnique(device);

    vk::ImageView attachments[2] = {colorAttachmentImage_.imageView(), depthStencilImage_.imageView()};
    vk::FramebufferCreateInfo fbci{{}, *renderPass_, 2, attachments, width, height, 1 };
    frameBuffer_ = device.createFramebufferUnique(fbci);
    */

    //modelToWorld = glm::rotate(modelToWorld, glm::radians(90.0f), glm::vec3(1, 0, 0));
    // This matrix converts between OpenGL perspective and Vulkan perspective.
    // It flips the Y axis and shrinks the Z value to [0,1]
    glm::mat4 leftHandCorrection(
      1.0f,  0.0f, 0.0f, 0.0f,
      0.0f, -1.0f, 0.0f, 0.0f,
      0.0f,  0.0f, 0.5f, 0.0f,
      0.0f,  0.0f, 0.5f, 1.0f
    );

    float fieldOfView = glm::radians(45.0f);
    cameraState_.cameraToPerspective = leftHandCorrection * glm::perspective(fieldOfView, (float)width_/height_, 0.1f, 10000.0f);

    auto cubeBytes = vku::loadFile(SOURCE_DIR "textures/okretnica.ktx");
    vku::KTXFileLayout ktx(cubeBytes.data(), cubeBytes.data()+cubeBytes.size());
    if (!ktx.ok()) {
      std::cout << "Could not load KTX file" << std::endl;
      exit(1);
    }

    cubeMap_ = vku::TextureImageCube{device, memprops, ktx.width(0), ktx.height(0), ktx.mipLevels(), vk::Format::eR8G8B8A8Unorm};
    ktx.upload(device, cubeMap_, cubeBytes, *commandPool_, memprops, queue);

    vku::SamplerMaker sm{};
    sm.magFilter(vk::Filter::eLinear);
    sm.minFilter(vk::Filter::eLinear);
    sm.mipmapMode(vk::SamplerMipmapMode::eNearest);
    cubeSampler_ = sm.createUnique(device);

    standardLayout_ = StandardLayout(device);

    dynamicsPipeline_ = DynamicsPipeline(device, ctxt.pipelineCache(), renderPass_, width_, height_, standardLayout_.pipelineLayout());

    fountPipeline_ = FountPipeline(device, ctxt.pipelineCache(), renderPass_, width_, height_, standardLayout_.pipelineLayout());

    vku::PipelineMaker pm{width, height};
    pm.topology(vk::PrimitiveTopology::ePointList);
    pm.depthTestEnable(VK_TRUE);
    solventPipeline_ = GraphicsPipeline(
      device, ctxt.pipelineCache(), renderPass_, standardLayout_.pipelineLayout(),
      BINARY_DIR "solvent.vert.spv",  BINARY_DIR "solvent.frag.spv", pm
    );

    skyboxPipeline_ = GraphicsPipeline(
      device, ctxt.pipelineCache(), renderPass_, width_, height_, standardLayout_.pipelineLayout(),
      BINARY_DIR "skybox.vert.spv",  BINARY_DIR "skybox.frag.spv"
    );

    atomPipeline_ = GraphicsPipeline(
      device, ctxt.pipelineCache(), renderPass_, width_, height_, standardLayout_.pipelineLayout(),
      BINARY_DIR "atoms.vert.spv",  BINARY_DIR "atoms.frag.spv"
    );

    connPipeline_ = GraphicsPipeline(
      device, ctxt.pipelineCache(), renderPass_, width_, height_, standardLayout_.pipelineLayout(),
      BINARY_DIR "conns.vert.spv",  BINARY_DIR "conns.frag.spv"
    );

    textModel_ = TextModel(FOUNT_NAME, device, memprops, *commandPool_, queue);
    //textModel_.draw(vec3(0), vec2(0), vec3(0, 0, 1), vec2(0.1f), "hello world");


    //moleculeModel_.updateDescriptorSet(device, standardLayout_.descriptorSetLayout(), ctxt.descriptorPool(), *cubeSampler_, cubeMap_.imageView(), textModel_.sampler(), textModel_.imageView(), textModel_.glyphs(), textModel_.maxGlyphs());

    for (int i = 0; i != Pick::fifoSize; ++i) {
      vk::EventCreateInfo eci{};
      pickEvents_.push_back(ctxt.device().createEventUnique(eci));
    }

    model_.updateDescriptorSet(device, standardLayout_.descriptorSetLayout(), ctxt.descriptorPool(), *cubeSampler_, cubeMap_.imageView(), textModel_.sampler(), textModel_.imageView(), textModel_.glyphs(), textModel_.maxGlyphs());

    glfwSetWindowUserPointer(glfwwindow_, (void*)this);
    glfwSetScrollCallback(glfwwindow_, scrollHandler);
    glfwSetMouseButtonCallback(glfwwindow_, mouseButtonHandlerHook);
    glfwSetKeyCallback(glfwwindow_, keyHandler);
  }

  boost::python::object render(Context &ctxt, Model &model) {
    return bp::object();
  }

  void simulate(Context &ctxt) {
  }

  void draw(Context &ctxt, vk::CommandBuffer cb, vk::RenderPassBeginInfo &rpbi) {
    auto device = ctxt.device();
    auto memprops = ctxt.memprops();
    auto queue = ctxt.queue();
    auto graphicsQueueFamilyIndex = ctxt.graphicsQueueFamilyIndex();

    float timeStep = 1.0f / 60;

    double xpos = 0, ypos = 0;
    glfwGetCursorPos(glfwwindow_, &xpos, &ypos);

    // Trackball rotation.
    if (mouseState_.rotating) {
      float dx = float(xpos - mouseState_.prevXpos);
      float dy = float(ypos - mouseState_.prevYpos);
      float dz = 0;
      float xspeed = 0.1f;
      float yspeed = 0.1f;
      float zspeed = 0.2f;
      float halfw = width_ * 0.5f;
      float halfh = height_ * 0.5f;
      float thresh = std::min(halfh, halfw) * 0.8f;
      float rx = float(xpos) - halfw;
      float ry = float(ypos) - halfh;
      float r = std::sqrt(rx*rx + ry*ry);
      float speed = std::sqrt(dx*dx + dy*dy);

      // Z rotation when outside inner circle
      if (r - thresh > 0 && speed > 0) {
        float tail = std::min((r - thresh) * (2.0f / thresh), 1.0f);
        dz = (dx * ry - dy * rx) * tail / std::sqrt(rx*rx + ry*ry);
        dx *= (1.0f - tail);
        dy *= (1.0f - tail);
      }
      glm::mat4 worldToModel = glm::inverse(moleculeState_.modelToWorld);
      glm::vec3 xaxis = worldToModel[0];
      glm::vec3 yaxis = worldToModel[1];
      glm::vec3 zaxis = worldToModel[2];
      auto &mat = moleculeState_.modelToWorld;
      mat = glm::rotate(mat, glm::radians(dy * yspeed), xaxis);
      mat = glm::rotate(mat, glm::radians(dx * xspeed), yaxis);
      mat = glm::rotate(mat, glm::radians(dz * zspeed), zaxis);
      mouseState_.prevXpos = xpos;
      mouseState_.prevYpos = ypos;
    }    

    if (0) {
      auto &mat = moleculeState_.modelToWorld;
      glm::mat4 worldToModel = glm::inverse(moleculeState_.modelToWorld);
      glm::vec3 xaxis = worldToModel[0];
      glm::vec3 yaxis = worldToModel[1];
      mat = glm::rotate(mat, glm::radians(3.0f), yaxis);
    }

    //moleculeState_.modelToWorld = glm::rotate(moleculeState_.modelToWorld, glm::radians(1.0f), glm::vec3(0, 1, 0));

    auto gfi = graphicsQueueFamilyIndex;

    vk::Event event = *pickEvents_[pickReadIndex_ & (Pick::fifoSize-1)];
    while (device.getEventStatus(event) == vk::Result::eEventSet) {
      Pick *pick = (Pick*)model_.pick().map(device);
      Pick &p = pick[pickReadIndex_ & (Pick::fifoSize-1)];
      moleculeState_.mouseAtom = p.atom;
      moleculeState_.mouseDistance = p.distance / 10000.0f;
      //printf("pick %d %d\n", p.atom, p.distance);
      model_.pick().unmap(device);
      p.distance = ~0;
      p.atom = ~0;
      device.resetEvent(event);
      pickReadIndex_++;
    }

    {
      Atom *atoms = model_.pAtoms();

      /*if (moleculeState_.dragging) {
        if (moleculeState_.selectedAtom < moleculeModel_.numAtoms()) {
          vec3 mousePos = modelCameraPos + modelMouseDir * moleculeState_.selectedDistance;
          vec3 atomPos = atoms[moleculeState_.selectedAtom].pos;
          //printf("%s %s\n", to_string(mousePos).c_str(), to_string(atomPos).c_str());
          vec3 axis = mousePos - atomPos;
          float f = length(axis) * 10.0f;
          atoms[moleculeState_.selectedAtom].acc += normalize(axis) * (f * timeStep);
        }
      }*/
    }
    glm::mat4 cameraToWorld = glm::translate(glm::mat4{}, glm::vec3(0, 0, cameraState_.cameraDistance));
    glm::mat4 modelToWorld = moleculeState_.modelToWorld;

    glm::mat4 worldToCamera = glm::inverse(cameraToWorld);
    glm::mat4 worldToModel = glm::inverse(modelToWorld);

    glm::vec3 worldCameraPos = cameraToWorld[3];
    glm::vec3 modelCameraPos = worldToModel * glm::vec4(worldCameraPos, 1);
    float xscreen = (float)xpos * 2.0f / width_ - 1.0f;
    float yscreen = (float)ypos * 2.0f / height_ - 1.0f;
    float tanfovX = 1.0f / cameraState_.cameraToPerspective[0][0];
    float tanfovY = 1.0f / cameraState_.cameraToPerspective[1][1];
    glm::vec4 cameraMouseDir = glm::vec4(xscreen * tanfovX, yscreen * tanfovY, -1, 0);
    glm::vec3 modelMouseDir = worldToModel * (cameraToWorld * cameraMouseDir);

    static int z = 0;
    float c = std::cos(z++ * 0.1f);
    PushConstants cu;
    //cu.timeStep = timeStep;
    cu.numAtoms = model_.numAtoms();
    cu.numConnections = model_.numConnections();
    cu.pickIndex = (pickWriteIndex_++) & (Pick::fifoSize-1);
    //cu.forceAtom = moleculeState_.selectedAtom;

    cu.rayStart = modelCameraPos;
    cu.rayDir = glm::normalize(modelMouseDir);

    cu.worldToPerspective = cameraState_.cameraToPerspective * worldToCamera;
    cu.modelToWorld = modelToWorld;
    cu.cameraToWorld = cameraToWorld;

    using psflags = vk::PipelineStageFlagBits;
    using aflags = vk::AccessFlagBits;

    uint32_t ninst = 1; //moleculeState_.showInstances ? model_.numInstances() : 1;


    vk::CommandBufferBeginInfo bi{};
    cb.begin(bi);

    // Do the physics velocity update on the GPU and the first pick pass
    cu.pass = 0;
    cb.pushConstants(standardLayout_.pipelineLayout(), vk::ShaderStageFlagBits::eAll, 0, sizeof(PushConstants), &cu);
    cb.bindDescriptorSets(vk::PipelineBindPoint::eCompute, standardLayout_.pipelineLayout(), 0, model_.descriptorSet(), nullptr);
    cb.bindPipeline(vk::PipelineBindPoint::eCompute, dynamicsPipeline_.pipeline());
    cb.dispatch(cu.numAtoms, ninst, 1);

    // Do the physics position update on the GPU and the second pick pass
    cu.pass = 1;
    cb.pushConstants(standardLayout_.pipelineLayout(), vk::ShaderStageFlagBits::eAll, 0, sizeof(PushConstants), &cu);
    cb.dispatch(cu.numAtoms, ninst, 1);

    /*
    moleculeModel_.atoms().barrier(
      cb, psflags::eComputeShader, psflags::eTopOfPipe, {},
      aflags::eShaderRead|aflags::eShaderWrite, aflags::eShaderRead, gfi, gfi
    );
    */

    // Signal the CPU that a pick event has occurred
    cb.setEvent(*pickEvents_[cu.pickIndex], vk::PipelineStageFlagBits::eComputeShader);

    std::array<float, 4> clearColorValue{0.75f, 0.75f, 0.75f, 1};
    vk::ClearDepthStencilValue clearDepthValue{ 1.0f, 0 };
    std::array<vk::ClearValue, 2> clearColours{vk::ClearValue{clearColorValue}, clearDepthValue};
    /*vk::RenderPassBeginInfo rpbi;
    rpbi.renderPass = *renderPass_;
    rpbi.framebuffer = *frameBuffer_;
    rpbi.renderArea = vk::Rect2D{{0, 0}, {width_, height_}};
    rpbi.clearValueCount = (uint32_t)clearColours.size();
    rpbi.pClearValues = clearColours.data();*/
    cb.beginRenderPass(rpbi, vk::SubpassContents::eInline);

    cb.pushConstants(standardLayout_.pipelineLayout(), vk::ShaderStageFlagBits::eAll, 0, sizeof(PushConstants), &cu);
    cb.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, standardLayout_.pipelineLayout(), 0, model_.descriptorSet(), nullptr);

    cb.bindPipeline(vk::PipelineBindPoint::eGraphics, skyboxPipeline_.pipeline());
    cb.draw(6 * 6, 1, 0, 0);

    /*cb.bindPipeline(vk::PipelineBindPoint::eGraphics, atomPipeline_.pipeline());
    if (model_.numAtoms()) cb.draw(model_.numAtoms() * 6, ninst, 0, 0);

    cb.bindPipeline(vk::PipelineBindPoint::eGraphics, connPipeline_.pipeline());
    if (model_.numConnections()) cb.draw(model_.numConnections() * 6, ninst, 0, 0);

    cb.bindPipeline(vk::PipelineBindPoint::eGraphics, fountPipeline_.pipeline());
    if (textModel_.numGlyphs()) cb.draw(textModel_.numGlyphs() * 6, ninst, 0, 0);
    */

    cb.bindPipeline(vk::PipelineBindPoint::eGraphics, solventPipeline_.pipeline());
    if (model_.numSolventAcessible()) cb.draw(model_.numSolventAcessible(), 1, 0, 0);

    cb.endRenderPass();

    //colorAttachmentImage_.setLayout(cb, vk::ImageLayout::eGeneral);
    //std::array<float, 4> cval{c, 0, 0, 1};
    //vk::ClearColorValue color{cval};
    //vk::ImageSubresourceRange range{vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1};
    //cb.clearColorImage( colorAttachmentImage_.image(), vk::ImageLayout::eGeneral, color, range);

    /*vk::BufferImageCopy region{
      0, 0, 0,
      {vk::ImageAspectFlagBits::eColor, 0, 0, 1},
      {0, 0, 0},
      {width_, height_, 1}
    };
    colorAttachmentImage_.setLayout(cb, vk::ImageLayout::eTransferSrcOptimal);
    cb.copyImageToBuffer( colorAttachmentImage_.image(), vk::ImageLayout::eTransferSrcOptimal, transferBuffer_.buffer(), region );
    */

    /*return boost::python::object(boost::python::handle<>(PyMemoryView_FromMemory(
      (char*)mappedTransferBuffer_, transferBuffer_.size(), PyBUF_READ))
    );*/

    cb.end();
  }

  static void scrollHandler(GLFWwindow *window, double dx, double dy) {
    View &app = *(View*)glfwGetWindowUserPointer(window);
    app.cameraState_.cameraDistance -= (float)dy * 4.0f;
  }
  static void mouseButtonHandlerHook(GLFWwindow *window, int button, int action, int mods) {
    View &app = *(View*)glfwGetWindowUserPointer(window);
    app.mouseButtonHandler(window, button, action, mods);
  }

  void mouseButtonHandler(GLFWwindow *window, int button, int action, int mods) {
    switch (button) {
      case GLFW_MOUSE_BUTTON_1: {
        if (action == GLFW_PRESS) {
          Atom *atoms = model_.pAtoms();
          int newStart = -1;
          int newEnd = -1;
          if (moleculeState_.mouseAtom == -1) {
            newStart = newEnd = -1;
          } else if (mods & GLFW_MOD_SHIFT) {
            if (moleculeState_.startAtom != -1) {
              auto startAtom = model_.pdbAtoms()[moleculeState_.startAtom];
              auto endAtom = model_.pdbAtoms()[moleculeState_.mouseAtom];
              //printf("%c %c\n", startAtom.chainID(), endAtom.chainID());
              if (startAtom.chainID() != endAtom.chainID()) {
                newStart = moleculeState_.startAtom;
                newEnd = moleculeState_.endAtom;
              } else {
                newStart = moleculeState_.startAtom;
                newEnd = moleculeState_.mouseAtom;
              }
            } else {
              newStart = newEnd = moleculeState_.mouseAtom;
            }
          } else {
            newStart = newEnd = moleculeState_.mouseAtom;
          }
          //printf("%d %d -> %d %d\n", moleculeState_.startAtom, moleculeState_.endAtom, newStart, newEnd);
          if (moleculeState_.startAtom != -1) {
            for (int i = moleculeState_.startAtom; i <= moleculeState_.endAtom; ++i) {
              atoms[i].selected = 0;
            }
          }
          if (newStart > newEnd) {
            std::swap(newStart, newEnd);
          }
          moleculeState_.startAtom = newStart;
          moleculeState_.endAtom = newEnd;
          if (moleculeState_.startAtom != -1) {
            for (int i = moleculeState_.startAtom; i <= moleculeState_.endAtom; ++i) {
              atoms[i].selected = 1;
            }
            //moleculeState_.dragging = true;
            moleculeState_.selectedDistance = moleculeState_.mouseDistance;
            /*textModel_.reset();
            auto atom = model_.pdbAtoms()[a];
            char buf[256];
            snprintf(buf, sizeof(buf), "%s %s %d (%d..%d)\n", atom.atomName().c_str(), atom.resName().c_str(), atom.resSeq(), a, moleculeState_.endAtom);
            vec3 pos = atoms[a].pos;
            textModel_.draw(pos, vec2(0), vec3(1, 1, 1), vec2(0.1f), buf);*/
          }
        } else {
          moleculeState_.dragging = false;
        }
      } break;
 
      case GLFW_MOUSE_BUTTON_2: {
        if (action == GLFW_PRESS) {
          mouseState_.rotating = true;
          glfwGetCursorPos(window, &mouseState_.prevXpos, &mouseState_.prevYpos);
        } else {
          mouseState_.rotating = false;
        }
      } break;
 
      case GLFW_MOUSE_BUTTON_3: {
      } break;
    }
  }
  static void keyHandler(GLFWwindow *window, int key, int scancode, int action, int mods) {
    View &app = *(View*)glfwGetWindowUserPointer(window);
    auto &pos = app.moleculeState_.modelToWorld[3];
    auto &cam = app.cameraState_.cameraRotation;
    // move the molecule along the camera x and y axis.
    if (action != GLFW_PRESS && action != GLFW_REPEAT) return;
    switch (key) {
      case GLFW_KEY_W: {
        app.cameraState_.cameraDistance -= 4;
      } break;
      case GLFW_KEY_S: {
        app.cameraState_.cameraDistance += 4;
      } break;
      case GLFW_KEY_LEFT: {
        pos += cam[0] * 0.5f;
      } break;
      case GLFW_KEY_RIGHT: {
        pos -= cam[0] * 0.5f;
      } break;
      case GLFW_KEY_UP: {
        pos -= cam[1] * 0.5f;
      } break;
      case GLFW_KEY_DOWN: {
        pos += cam[1] * 0.5f;
      } break;
      case '[': {
        app.rotateSelected(1);
      } break;
      case ']': {
        app.rotateSelected(-1);
      } break;
      case ',': {
        app.translateSelected(1);
      } break;
      case '.': {
        app.translateSelected(-1);
      } break;
      case '=': {
        app.moleculeState_.showInstances = !app.moleculeState_.showInstances;
      } break;
    }
  }
  void rotateSelected(int dir) {
    Atom *atoms = model_.pAtoms();
    //mat4 xform = glm::translate(mat4, 
    if (moleculeState_.startAtom != -1) {
      vec3 pos1 = atoms[moleculeState_.startAtom].pos;
      vec3 pos2 = atoms[moleculeState_.endAtom].pos;
      vec3 axis = glm::normalize(pos2 - pos1);
      mat4 rotate = glm::rotate(mat4{}, glm::radians(2.0f * dir), axis);
      for (int i = moleculeState_.startAtom; i <= moleculeState_.endAtom; ++i) {
        atoms[i].pos = vec3(rotate * vec4(atoms[i].pos - pos1, 1)) + pos1;
      }
    }
  }
  void translateSelected(int dir) {
    Atom *atoms = model_.pAtoms();
    if (moleculeState_.startAtom != -1) {
      for (int i = moleculeState_.startAtom; i <= moleculeState_.endAtom; ++i) {
        atoms[i].pos.x += dir;
      }
    }
  }

  View(const View &rhs) : model_(*(Model*)nullptr) {}
  void operator=(const View &rhs) {}

  View(View &&rhs) = default;
  View &operator=(View &&rhs) = default;

  bool poll(Context &ctxt) {
    if (glfwWindowShouldClose(glfwwindow_)) {
      return false;
    }

    auto device = ctxt.device();
    auto queue = ctxt.queue();
    window_.draw(
      device, queue,
      [&](vk::CommandBuffer cb, int imageIndex, vk::RenderPassBeginInfo &rpbi) { draw(ctxt, cb, rpbi); }
    );

    glfwPollEvents();
    return true;
  }
private:
  vk::RenderPass renderPass_;
  vku::DepthStencilImage depthStencilImage_;
  vku::ColorAttachmentImage colorAttachmentImage_;
  vk::UniqueFramebuffer frameBuffer_;
  vku::GenericBuffer transferBuffer_;
  vk::UniqueCommandPool commandPool_;
  uint32_t width_;
  uint32_t height_;
  void *mappedTransferBuffer_;

  vku::Window window_;
  GLFWwindow *glfwwindow_;

  StandardLayout standardLayout_;
  FountPipeline fountPipeline_;

  DynamicsPipeline dynamicsPipeline_;

  GraphicsPipeline atomPipeline_;
  GraphicsPipeline connPipeline_;
  GraphicsPipeline skyboxPipeline_;
  GraphicsPipeline solventPipeline_;

  TextModel textModel_;

  vku::TextureImageCube cubeMap_;
  vk::UniqueSampler cubeSampler_;

  struct MouseState {
    double prevXpos = 0;
    double prevYpos = 0;
    bool rotating = false;
  };
  MouseState mouseState_;

  //GLFWwindow *glfwwindow_;
  vk::Device device_;

  struct MoleculeState {
    glm::mat4 modelToWorld;
    int startAtom = -1;
    int endAtom = -1;
    int mouseAtom;
    float selectedDistance;
    float mouseDistance;
    bool dragging = false;
    bool showInstances = false;
  };
  MoleculeState moleculeState_;

  struct CameraState {
    glm::mat4 cameraRotation;
    glm::mat4 cameraToPerspective;
    float cameraDistance = 50.0f;
  };
  CameraState cameraState_;

  uint32_t pickWriteIndex_ = 0;
  uint32_t pickReadIndex_ = 0;

  std::vector<vk::UniqueEvent> pickEvents_;

  Model &model_;
};

inline void Context::mainloop() {
  for (;;) {
    for (auto v : views_) {
      if (!v->poll(*this)) {
        return;
      }
    }

    // Very crude method to prevent your GPU from overheating.
    std::this_thread::sleep_for(std::chrono::milliseconds(16));
  }
  device_.waitIdle();
}

} // namespace moovoo


BOOST_PYTHON_MODULE(moovoo)
{
  namespace bp = boost::python;
  using namespace boost::python;
  using namespace moovoo;
  class_<Context>("Context", init<>())
    .def("mainloop", &Context::mainloop)
  ;
  class_<View>("View", init<Context &, const std::string &, bp::object&, const bp::object&>())
    .def("render", &View::render)
  ;
  class_<Model>("Model", init<Context &, bp::object &>())
  ;
}

