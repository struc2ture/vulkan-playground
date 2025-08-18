def main():
    func_call = "ImGui::BulletText"
    obj_name = "p.limits"
    parser = Parser(Lexer(src).tokenize())

    while parser.peek().kind != 'EOF':
        token = parser.advance()
        if token.kind == 'IDENT':
            TYPES = {
                'uint32_t': '%u',
                'int32_t': '%d',
                'VkDeviceSize': '%llu',
                'float': '%0.3f',
                'VkBool32': '%s',
                'VkSampleCountFlags': '%s'
            }
            if token.value in TYPES:
                var_typ = token.value
                format = TYPES[var_typ]
                var_name = parser.expect('IDENT').value
                next_next_token = parser.expect('LBRACKET', optional=True)
                is_array = next_next_token != None
                if is_array:
                    array_count = int(parser.expect('NUMBER').value)
                    print(f'{func_call}("{var_name} = [{", ".join([format] * array_count)}]", {", ".join(f"{obj_name}.{var_name}[{index}]" for index in range(array_count))});')
                elif var_typ == 'VkBool32':
                    print(f'{func_call}("{var_name} = {format}", {obj_name}.{var_name} ? "true" : "false");')
                elif var_typ == 'VkSampleCountFlags':
                    print(f'{func_call}("{var_name} = {format}", get_vk_sample_count_flag_names({obj_name}.{var_name}));')
                else:
                    print(f'{func_call}("{var_name} = {format}", {obj_name}.{var_name});')


class Token:
    def __init__(self, kind, value):
        self.kind = kind
        self.value = value

    def __repr__(self):
        return f"Token({self.kind}, {self.value})"

class Lexer:
    def __init__(self, src):
        self.src = src
        self.pos = 0

    def peek(self):
        return self.src[self.pos] if self.pos < len(self.src) else '\0'

    def advance(self):
        ch = self.peek()
        self.pos += 1
        return ch

    def skip_whitespace(self):
        while self.peek() in ' \r\t\n':
            self.advance()

    def match_keyword_or_indent(self, first):
        ident = first
        while self.peek().isalnum() or self.peek() == '_':
            ident += self.advance()
        KEYWORDS = {
            'typedef'
            'struct'
        }
        if ident in KEYWORDS:
            return Token(ident.upper(), ident)
        return Token('IDENT', ident)
    
    def match_number(self, first):
        num = first
        while self.peek().isdigit():
            num += self.advance()
        return Token('NUMBER', num)

    def next_token(self):
        self.skip_whitespace()
        ch = self.advance()

        if ch.isalpha() or ch == '_':
            return self.match_keyword_or_indent(ch)
        
        if ch.isdigit():
            return self.match_number(ch)

        SINGLE_CH_TOKENS = {
            '{': 'LBRACE',
            '}': 'RBRACE',
            ';': 'SEMICOLON',
            '[': 'LBRACKET',
            ']': 'RBRACKET'
        }

        if ch in SINGLE_CH_TOKENS:
            return Token(SINGLE_CH_TOKENS[ch], ch)

        if ch == '\0':
            return Token('EOF', '')

        raise SyntaxError(f"Unexpected character: {ch}")

    def tokenize(self):
        tokens = []
        while True:
            token = self.next_token()
            tokens.append(token)
            if token.kind == 'EOF':
                break
        return tokens

class Parser:
    def __init__(self, tokens):
        self.tokens = tokens
        self.pos = 0

    def peek(self):
        return self.tokens[self.pos]

    def peek2(self):
        return self.tokens[self.pos + 1] if self.pos + 1 < len(self.tokens) else Token('EOF', '')

    def advance(self):
        token = self.peek()
        self.pos += 1
        return token

    def expect(self, kind, optional=False):
        token = self.peek()
        if token.kind != kind and not optional:
            raise SyntaxError(f"Expected {kind}, got {token.kind}")
        if token.kind == kind:
            self.advance()
            return token
        else:
            return None

src = '''
typedef struct VkPhysicalDeviceLimits {
    uint32_t              maxImageDimension1D;
    uint32_t              maxImageDimension2D;
    uint32_t              maxImageDimension3D;
    uint32_t              maxImageDimensionCube;
    uint32_t              maxImageArrayLayers;
    uint32_t              maxTexelBufferElements;
    uint32_t              maxUniformBufferRange;
    uint32_t              maxStorageBufferRange;
    uint32_t              maxPushConstantsSize;
    uint32_t              maxMemoryAllocationCount;
    uint32_t              maxSamplerAllocationCount;
    VkDeviceSize          bufferImageGranularity;
    VkDeviceSize          sparseAddressSpaceSize;
    uint32_t              maxBoundDescriptorSets;
    uint32_t              maxPerStageDescriptorSamplers;
    uint32_t              maxPerStageDescriptorUniformBuffers;
    uint32_t              maxPerStageDescriptorStorageBuffers;
    uint32_t              maxPerStageDescriptorSampledImages;
    uint32_t              maxPerStageDescriptorStorageImages;
    uint32_t              maxPerStageDescriptorInputAttachments;
    uint32_t              maxPerStageResources;
    uint32_t              maxDescriptorSetSamplers;
    uint32_t              maxDescriptorSetUniformBuffers;
    uint32_t              maxDescriptorSetUniformBuffersDynamic;
    uint32_t              maxDescriptorSetStorageBuffers;
    uint32_t              maxDescriptorSetStorageBuffersDynamic;
    uint32_t              maxDescriptorSetSampledImages;
    uint32_t              maxDescriptorSetStorageImages;
    uint32_t              maxDescriptorSetInputAttachments;
    uint32_t              maxVertexInputAttributes;
    uint32_t              maxVertexInputBindings;
    uint32_t              maxVertexInputAttributeOffset;
    uint32_t              maxVertexInputBindingStride;
    uint32_t              maxVertexOutputComponents;
    uint32_t              maxTessellationGenerationLevel;
    uint32_t              maxTessellationPatchSize;
    uint32_t              maxTessellationControlPerVertexInputComponents;
    uint32_t              maxTessellationControlPerVertexOutputComponents;
    uint32_t              maxTessellationControlPerPatchOutputComponents;
    uint32_t              maxTessellationControlTotalOutputComponents;
    uint32_t              maxTessellationEvaluationInputComponents;
    uint32_t              maxTessellationEvaluationOutputComponents;
    uint32_t              maxGeometryShaderInvocations;
    uint32_t              maxGeometryInputComponents;
    uint32_t              maxGeometryOutputComponents;
    uint32_t              maxGeometryOutputVertices;
    uint32_t              maxGeometryTotalOutputComponents;
    uint32_t              maxFragmentInputComponents;
    uint32_t              maxFragmentOutputAttachments;
    uint32_t              maxFragmentDualSrcAttachments;
    uint32_t              maxFragmentCombinedOutputResources;
    uint32_t              maxComputeSharedMemorySize;
    uint32_t              maxComputeWorkGroupCount[3];
    uint32_t              maxComputeWorkGroupInvocations;
    uint32_t              maxComputeWorkGroupSize[3];
    uint32_t              subPixelPrecisionBits;
    uint32_t              subTexelPrecisionBits;
    uint32_t              mipmapPrecisionBits;
    uint32_t              maxDrawIndexedIndexValue;
    uint32_t              maxDrawIndirectCount;
    float                 maxSamplerLodBias;
    float                 maxSamplerAnisotropy;
    uint32_t              maxViewports;
    uint32_t              maxViewportDimensions[2];
    float                 viewportBoundsRange[2];
    uint32_t              viewportSubPixelBits;
    size_t                minMemoryMapAlignment;
    VkDeviceSize          minTexelBufferOffsetAlignment;
    VkDeviceSize          minUniformBufferOffsetAlignment;
    VkDeviceSize          minStorageBufferOffsetAlignment;
    int32_t               minTexelOffset;
    uint32_t              maxTexelOffset;
    int32_t               minTexelGatherOffset;
    uint32_t              maxTexelGatherOffset;
    float                 minInterpolationOffset;
    float                 maxInterpolationOffset;
    uint32_t              subPixelInterpolationOffsetBits;
    uint32_t              maxFramebufferWidth;
    uint32_t              maxFramebufferHeight;
    uint32_t              maxFramebufferLayers;
    VkSampleCountFlags    framebufferColorSampleCounts;
    VkSampleCountFlags    framebufferDepthSampleCounts;
    VkSampleCountFlags    framebufferStencilSampleCounts;
    VkSampleCountFlags    framebufferNoAttachmentsSampleCounts;
    uint32_t              maxColorAttachments;
    VkSampleCountFlags    sampledImageColorSampleCounts;
    VkSampleCountFlags    sampledImageIntegerSampleCounts;
    VkSampleCountFlags    sampledImageDepthSampleCounts;
    VkSampleCountFlags    sampledImageStencilSampleCounts;
    VkSampleCountFlags    storageImageSampleCounts;
    uint32_t              maxSampleMaskWords;
    VkBool32              timestampComputeAndGraphics;
    float                 timestampPeriod;
    uint32_t              maxClipDistances;
    uint32_t              maxCullDistances;
    uint32_t              maxCombinedClipAndCullDistances;
    uint32_t              discreteQueuePriorities;
    float                 pointSizeRange[2];
    float                 lineWidthRange[2];
    float                 pointSizeGranularity;
    float                 lineWidthGranularity;
    VkBool32              strictLines;
    VkBool32              standardSampleLocations;
    VkDeviceSize          optimalBufferCopyOffsetAlignment;
    VkDeviceSize          optimalBufferCopyRowPitchAlignment;
    VkDeviceSize          nonCoherentAtomSize;
} VkPhysicalDeviceLimits;
'''

if __name__ == "__main__":
    main()
