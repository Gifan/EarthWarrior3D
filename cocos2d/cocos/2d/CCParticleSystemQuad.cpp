/****************************************************************************
Copyright (c) 2008-2010 Ricardo Quesada
Copyright (c) 2009      Leonardo Kasperavičius
Copyright (c) 2010-2012 cocos2d-x.org
Copyright (c) 2011      Zynga Inc.
Copyright (c) 2013-2014 Chukong Technologies Inc.

http://www.cocos2d-x.org
 
Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
****************************************************************************/

#include "CCGL.h"
#include "CCParticleSystemQuad.h"
#include "CCSpriteFrame.h"
#include "CCDirector.h"
#include "CCParticleBatchNode.h"
#include "CCTextureAtlas.h"
#include "CCShaderCache.h"
#include "ccGLStateCache.h"
#include "CCGLProgram.h"
#include "TransformUtils.h"
#include "CCEventType.h"
#include "CCConfiguration.h"
#include "renderer/CCRenderer.h"
#include "renderer/CCQuadCommand.h"
#include "renderer/CCCustomCommand.h"

// extern
#include "kazmath/GL/matrix.h"
#include "CCEventListenerCustom.h"
#include "CCEventDispatcher.h"

NS_CC_BEGIN

//implementation ParticleSystemQuad
// overriding the init method
bool ParticleSystemQuad::initWithTotalParticles(int numberOfParticles)
{
    // base initialization
    if( ParticleSystem::initWithTotalParticles(numberOfParticles) ) 
    {
        // allocating data space
        if( ! this->allocMemory() ) {
            this->release();
            return false;
        }

        initIndices();
        if (Configuration::getInstance()->supportsShareableVAO())
        {
            setupVBOandVAO();
        }
        else
        {
            setupVBO();
        }

        setShaderProgram(ShaderCache::getInstance()->getProgram(GLProgram::SHADER_NAME_POSITION_TEXTURE_COLOR_NO_MVP));
        
#if CC_ENABLE_CACHE_TEXTURE_DATA
        // Need to listen the event only when not use batchnode, because it will use VBO
        auto listener = EventListenerCustom::create(EVENT_COME_TO_FOREGROUND, CC_CALLBACK_1(ParticleSystemQuad::listenBackToForeground, this));
        _eventDispatcher->addEventListenerWithSceneGraphPriority(listener, this);
#endif

        return true;
    }
    return false;
}

ParticleSystemQuad::ParticleSystemQuad()
:_quads(nullptr)
,_indices(nullptr)
,_VAOname(0)
{
    memset(_buffersVBO, 0, sizeof(_buffersVBO));
}

ParticleSystemQuad::~ParticleSystemQuad()
{
    if (nullptr == _batchNode)
    {
        CC_SAFE_FREE(_quads);
        CC_SAFE_FREE(_indices);
        glDeleteBuffers(2, &_buffersVBO[0]);
        if (Configuration::getInstance()->supportsShareableVAO())
        {
            glDeleteVertexArrays(1, &_VAOname);
            GL::bindVAO(0);
        }
    }
}

// implementation ParticleSystemQuad

ParticleSystemQuad * ParticleSystemQuad::create(const std::string& filename)
{
    ParticleSystemQuad *ret = new ParticleSystemQuad();
    if (ret && ret->initWithFile(filename))
    {
        ret->autorelease();
        return ret;
    }
    CC_SAFE_DELETE(ret);
    return ret;
}
ParticleSystemQuad * ParticleSystemQuad::create( ValueMap& map)
{
    ParticleSystemQuad *ret = new ParticleSystemQuad();
    if (ret && ret->initWithDictionary(map))
    {
        ret->autorelease();
        return ret;
    }
    CC_SAFE_DELETE(ret);
    return ret;
}
ParticleSystemQuad * ParticleSystemQuad::create( ValueMap& map, SpriteFrame *frame)
{
    ParticleSystemQuad *ret = new ParticleSystemQuad();
    if (ret && ret->initWithDictionaryAndFrame(map, frame))
    {
        ret->autorelease();
        return ret;
    }
    CC_SAFE_DELETE(ret);
    return ret;
}
ParticleSystemQuad * ParticleSystemQuad::createWithTotalParticles(int numberOfParticles) {
    ParticleSystemQuad *ret = new ParticleSystemQuad();
    if (ret && ret->initWithTotalParticles(numberOfParticles))
    {
        ret->autorelease();
        return ret;
    }
    CC_SAFE_DELETE(ret);
    return ret;
}

bool ParticleSystemQuad::initWithDictionaryAndFrame(ValueMap &dictionary, SpriteFrame* frame)
{
    std::string dirname = "";
    bool ret = false;
    unsigned char *buffer = nullptr;
    unsigned char *deflated = nullptr;
    //Image *image = nullptr;
    do
    {
        int maxParticles = dictionary["maxParticles"].asInt();
        // self, not super
        if(this->initWithTotalParticles(maxParticles))
        {
            // Emitter name in particle designer 2.0
            _configName = dictionary["configName"].asString();
            
            // angle
            _angle = dictionary["angle"].asFloat();
            _angleVar = dictionary["angleVariance"].asFloat();
            
            // duration
            _duration = dictionary["duration"].asFloat();
            
            // blend function
            if (_configName.length()>0)
            {
                _blendFunc.src = dictionary["blendFuncSource"].asFloat();
            }
            else
            {
                _blendFunc.src = dictionary["blendFuncSource"].asInt();
            }
            _blendFunc.dst = dictionary["blendFuncDestination"].asInt();
            
            // color
            _startColor.r = dictionary["startColorRed"].asFloat();
            _startColor.g = dictionary["startColorGreen"].asFloat();
            _startColor.b = dictionary["startColorBlue"].asFloat();
            _startColor.a = dictionary["startColorAlpha"].asFloat();
            
            _startColorVar.r = dictionary["startColorVarianceRed"].asFloat();
            _startColorVar.g = dictionary["startColorVarianceGreen"].asFloat();
            _startColorVar.b = dictionary["startColorVarianceBlue"].asFloat();
            _startColorVar.a = dictionary["startColorVarianceAlpha"].asFloat();
            
            _endColor.r = dictionary["finishColorRed"].asFloat();
            _endColor.g = dictionary["finishColorGreen"].asFloat();
            _endColor.b = dictionary["finishColorBlue"].asFloat();
            _endColor.a = dictionary["finishColorAlpha"].asFloat();
            
            _endColorVar.r = dictionary["finishColorVarianceRed"].asFloat();
            _endColorVar.g = dictionary["finishColorVarianceGreen"].asFloat();
            _endColorVar.b = dictionary["finishColorVarianceBlue"].asFloat();
            _endColorVar.a = dictionary["finishColorVarianceAlpha"].asFloat();
            
            // particle size
            _startSize = dictionary["startParticleSize"].asFloat();
            _startSizeVar = dictionary["startParticleSizeVariance"].asFloat();
            _endSize = dictionary["finishParticleSize"].asFloat();
            _endSizeVar = dictionary["finishParticleSizeVariance"].asFloat();
            
            // position
            float x = dictionary["sourcePositionx"].asFloat();
            float y = dictionary["sourcePositiony"].asFloat();
            this->setPosition( Point(x,y) );
            _posVar.x = dictionary["sourcePositionVariancex"].asFloat();
            _posVar.y = dictionary["sourcePositionVariancey"].asFloat();
            
            // Spinning
            _startSpin = dictionary["rotationStart"].asFloat();
            _startSpinVar = dictionary["rotationStartVariance"].asFloat();
            _endSpin= dictionary["rotationEnd"].asFloat();
            _endSpinVar= dictionary["rotationEndVariance"].asFloat();
            
            _emitterMode = (Mode) dictionary["emitterType"].asInt();
            
            // Mode A: Gravity + tangential accel + radial accel
            if (_emitterMode == Mode::GRAVITY)
            {
                // gravity
                modeA.gravity.x = dictionary["gravityx"].asFloat();
                modeA.gravity.y = dictionary["gravityy"].asFloat();
                
                // speed
                modeA.speed = dictionary["speed"].asFloat();
                modeA.speedVar = dictionary["speedVariance"].asFloat();
                
                // radial acceleration
                modeA.radialAccel = dictionary["radialAcceleration"].asFloat();
                modeA.radialAccelVar = dictionary["radialAccelVariance"].asFloat();
                
                // tangential acceleration
                modeA.tangentialAccel = dictionary["tangentialAcceleration"].asFloat();
                modeA.tangentialAccelVar = dictionary["tangentialAccelVariance"].asFloat();
                
                // rotation is dir
                modeA.rotationIsDir = dictionary["rotationIsDir"].asBool();
            }
            
            // or Mode B: radius movement
            else if (_emitterMode == Mode::RADIUS)
            {
                if (_configName.length()>0)
                {
                    modeB.startRadius = dictionary["maxRadius"].asInt();
                }
                else
                {
                    modeB.startRadius = dictionary["maxRadius"].asFloat();
                }
                modeB.startRadiusVar = dictionary["maxRadiusVariance"].asFloat();
                if (_configName.length()>0)
                {
                    modeB.endRadius = dictionary["minRadius"].asInt();
                }
                else
                {
                    modeB.endRadius = dictionary["minRadius"].asFloat();
                }
                modeB.endRadiusVar = 0.0f;
                if (_configName.length()>0)
                {
                    modeB.rotatePerSecond = dictionary["rotatePerSecond"].asInt();
                }
                else
                {
                    modeB.rotatePerSecond = dictionary["rotatePerSecond"].asFloat();
                }
                modeB.rotatePerSecondVar = dictionary["rotatePerSecondVariance"].asFloat();
                
            } else {
                CCASSERT( false, "Invalid emitterType in config file");
                CC_BREAK_IF(true);
            }
            
            // life span
            _life = dictionary["particleLifespan"].asFloat();
            _lifeVar = dictionary["particleLifespanVariance"].asFloat();
            
            // emission Rate
            _emissionRate = _totalParticles / _life;
            
            //don't get the internal texture if a batchNode is used
            if (!_batchNode)
            {
                // Set a compatible default for the alpha transfer
                _opacityModifyRGB = false;
                
                
                if (!_configName.empty())
                {
                    _yCoordFlipped = dictionary["yCoordFlipped"].asInt();
                }
                
            }
            setDisplayFrame(frame);
            ret = true;
        }
    } while (0);
    free(buffer);
    free(deflated);
    return ret;
    
}

// pointRect should be in Texture coordinates, not pixel coordinates
void ParticleSystemQuad::initTexCoordsWithRect(const Rect& pointRect)
{
    // convert to Tex coords

    Rect rect = Rect(
        pointRect.origin.x * CC_CONTENT_SCALE_FACTOR(),
        pointRect.origin.y * CC_CONTENT_SCALE_FACTOR(),
        pointRect.size.width * CC_CONTENT_SCALE_FACTOR(),
        pointRect.size.height * CC_CONTENT_SCALE_FACTOR());

    GLfloat wide = (GLfloat) pointRect.size.width;
    GLfloat high = (GLfloat) pointRect.size.height;

    if (_texture)
    {
        wide = (GLfloat)_texture->getPixelsWide();
        high = (GLfloat)_texture->getPixelsHigh();
    }

#if CC_FIX_ARTIFACTS_BY_STRECHING_TEXEL
    GLfloat left = (rect.origin.x*2+1) / (wide*2);
    GLfloat bottom = (rect.origin.y*2+1) / (high*2);
    GLfloat right = left + (rect.size.width*2-2) / (wide*2);
    GLfloat top = bottom + (rect.size.height*2-2) / (high*2);
#else
    GLfloat left = rect.origin.x / wide;
    GLfloat bottom = rect.origin.y / high;
    GLfloat right = left + rect.size.width / wide;
    GLfloat top = bottom + rect.size.height / high;
#endif // ! CC_FIX_ARTIFACTS_BY_STRECHING_TEXEL

    // Important. Texture in cocos2d are inverted, so the Y component should be inverted
    CC_SWAP( top, bottom, float);

    V3F_C4B_T2F_Quad *quads = nullptr;
    unsigned int start = 0, end = 0;
    if (_batchNode)
    {
        quads = _batchNode->getTextureAtlas()->getQuads();
        start = _atlasIndex;
        end = _atlasIndex + _totalParticles;
    }
    else
    {
        quads = _quads;
        start = 0;
        end = _totalParticles;
    }

    for(unsigned int i=start; i<end; i++) 
    {
        // bottom-left vertex:
        quads[i].bl.texCoords.u = left;
        quads[i].bl.texCoords.v = bottom;
        // bottom-right vertex:
        quads[i].br.texCoords.u = right;
        quads[i].br.texCoords.v = bottom;
        // top-left vertex:
        quads[i].tl.texCoords.u = left;
        quads[i].tl.texCoords.v = top;
        // top-right vertex:
        quads[i].tr.texCoords.u = right;
        quads[i].tr.texCoords.v = top;
    }
}

void ParticleSystemQuad::updateTexCoords()
{
    if (_texture)
    {
        const Size& s = _texture->getContentSize();
        initTexCoordsWithRect(Rect(0, 0, s.width, s.height));
    }
}

void ParticleSystemQuad::setTextureWithRect(Texture2D *texture, const Rect& rect)
{
    // Only update the texture if is different from the current one
    if( !_texture || texture->getName() != _texture->getName() )
    {
        ParticleSystem::setTexture(texture);
    }

    this->initTexCoordsWithRect(rect);
}

void ParticleSystemQuad::setTexture(Texture2D* texture)
{
    const Size& s = texture->getContentSize();
    this->setTextureWithRect(texture, Rect(0, 0, s.width, s.height));
}

void ParticleSystemQuad::setDisplayFrame(SpriteFrame *spriteFrame)
{
    CCASSERT(spriteFrame->getOffsetInPixels().equals(Point::ZERO), 
             "QuadParticle only supports SpriteFrames with no offsets");

    // update texture before updating texture rect
    if ( !_texture || spriteFrame->getTexture()->getName() != _texture->getName())
    {
        //this->setTexture(spriteFrame->getTexture());
        this->setTextureWithRect(spriteFrame->getTexture(), spriteFrame->getRect());
    }
}

void ParticleSystemQuad::initIndices()
{
    for(int i = 0; i < _totalParticles; ++i)
    {
        const unsigned int i6 = i*6;
        const unsigned int i4 = i*4;
        _indices[i6+0] = (GLushort) i4+0;
        _indices[i6+1] = (GLushort) i4+1;
        _indices[i6+2] = (GLushort) i4+2;

        _indices[i6+5] = (GLushort) i4+1;
        _indices[i6+4] = (GLushort) i4+2;
        _indices[i6+3] = (GLushort) i4+3;
    }
}

void ParticleSystemQuad::updateQuadWithParticle(tParticle* particle, const Point& newPosition)
{
    V3F_C4B_T2F_Quad *quad;

    if (_batchNode)
    {
        V3F_C4B_T2F_Quad *batchQuads = _batchNode->getTextureAtlas()->getQuads();
        quad = &(batchQuads[_atlasIndex+particle->atlasIndex]);
    }
    else
    {
        quad = &(_quads[_particleIdx]);
    }
    Color4B color = (_opacityModifyRGB)
        ? Color4B( particle->color.r*particle->color.a*255, particle->color.g*particle->color.a*255, particle->color.b*particle->color.a*255, particle->color.a*255)
        : Color4B( particle->color.r*255, particle->color.g*255, particle->color.b*255, particle->color.a*255);

    quad->bl.colors = color;
    quad->br.colors = color;
    quad->tl.colors = color;
    quad->tr.colors = color;

    // vertices
    GLfloat size_2 = particle->size/2;
    if (particle->rotation) 
    {
        GLfloat x1 = -size_2;
        GLfloat y1 = -size_2;

        GLfloat x2 = size_2;
        GLfloat y2 = size_2;
        GLfloat x = newPosition.x;
        GLfloat y = newPosition.y;

        GLfloat r = (GLfloat)-CC_DEGREES_TO_RADIANS(particle->rotation);
        GLfloat cr = cosf(r);
        GLfloat sr = sinf(r);
        GLfloat ax = x1 * cr - y1 * sr + x;
        GLfloat ay = x1 * sr + y1 * cr + y;
        GLfloat bx = x2 * cr - y1 * sr + x;
        GLfloat by = x2 * sr + y1 * cr + y;
        GLfloat cx = x2 * cr - y2 * sr + x;
        GLfloat cy = x2 * sr + y2 * cr + y;
        GLfloat dx = x1 * cr - y2 * sr + x;
        GLfloat dy = x1 * sr + y2 * cr + y;

        // bottom-left
        quad->bl.vertices.x = ax;
        quad->bl.vertices.y = ay;

        // bottom-right vertex:
        quad->br.vertices.x = bx;
        quad->br.vertices.y = by;

        // top-left vertex:
        quad->tl.vertices.x = dx;
        quad->tl.vertices.y = dy;

        // top-right vertex:
        quad->tr.vertices.x = cx;
        quad->tr.vertices.y = cy;
    } 
    else 
    {
        // bottom-left vertex:
        quad->bl.vertices.x = newPosition.x - size_2;
        quad->bl.vertices.y = newPosition.y - size_2;

        // bottom-right vertex:
        quad->br.vertices.x = newPosition.x + size_2;
        quad->br.vertices.y = newPosition.y - size_2;

        // top-left vertex:
        quad->tl.vertices.x = newPosition.x - size_2;
        quad->tl.vertices.y = newPosition.y + size_2;

        // top-right vertex:
        quad->tr.vertices.x = newPosition.x + size_2;
        quad->tr.vertices.y = newPosition.y + size_2;                
    }
}
void ParticleSystemQuad::postStep()
{
    glBindBuffer(GL_ARRAY_BUFFER, _buffersVBO[0]);
    
    // Option 1: Sub Data
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(_quads[0])*_totalParticles, _quads);
    
    // Option 2: Data
    //  glBufferData(GL_ARRAY_BUFFER, sizeof(quads_[0]) * particleCount, quads_, GL_DYNAMIC_DRAW);
    
    // Option 3: Orphaning + glMapBuffer
    // glBufferData(GL_ARRAY_BUFFER, sizeof(_quads[0])*_totalParticles, nullptr, GL_STREAM_DRAW);
    // void *buf = glMapBuffer(GL_ARRAY_BUFFER, GL_WRITE_ONLY);
    // memcpy(buf, _quads, sizeof(_quads[0])*_totalParticles);
    // glUnmapBuffer(GL_ARRAY_BUFFER);
    
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    
    CHECK_GL_ERROR_DEBUG();
}

// overriding draw method
void ParticleSystemQuad::draw(Renderer *renderer, const kmMat4 &transform, bool transformUpdated)
{
    CCASSERT( _particleIdx == 0 || _particleIdx == _particleCount, "Abnormal error in particle quad");
    //quad command
    if(_particleIdx > 0)
    {
        _quadCommand.init(_globalZOrder, _texture->getName(), _shaderProgram, _blendFunc, _quads, _particleIdx, transform);
        renderer->addCommand(&_quadCommand);
    }
}

void ParticleSystemQuad::setTotalParticles(int tp)
{
    // If we are setting the total number of particles to a number higher
    // than what is allocated, we need to allocate new arrays
    if( tp > _allocatedParticles )
    {
        // Allocate new memory
        size_t particlesSize = tp * sizeof(tParticle);
        size_t quadsSize = sizeof(_quads[0]) * tp * 1;
        size_t indicesSize = sizeof(_indices[0]) * tp * 6 * 1;

        tParticle* particlesNew = (tParticle*)realloc(_particles, particlesSize);
        V3F_C4B_T2F_Quad* quadsNew = (V3F_C4B_T2F_Quad*)realloc(_quads, quadsSize);
        GLushort* indicesNew = (GLushort*)realloc(_indices, indicesSize);

        if (particlesNew && quadsNew && indicesNew)
        {
            // Assign pointers
            _particles = particlesNew;
            _quads = quadsNew;
            _indices = indicesNew;

            // Clear the memory
            memset(_particles, 0, particlesSize);
            memset(_quads, 0, quadsSize);
            memset(_indices, 0, indicesSize);
            
            _allocatedParticles = tp;
        }
        else
        {
            // Out of memory, failed to resize some array
            if (particlesNew) _particles = particlesNew;
            if (quadsNew) _quads = quadsNew;
            if (indicesNew) _indices = indicesNew;

            CCLOG("Particle system: out of memory");
            return;
        }

        _totalParticles = tp;

        // Init particles
        if (_batchNode)
        {
            for (int i = 0; i < _totalParticles; i++)
            {
                _particles[i].atlasIndex=i;
            }
        }

        initIndices();
        if (Configuration::getInstance()->supportsShareableVAO())
        {
            setupVBOandVAO();
        }
        else
        {
            setupVBO();
        }
        
        // fixed http://www.cocos2d-x.org/issues/3990
        // Updates texture coords.
        updateTexCoords();
    }
    else
    {
        _totalParticles = tp;
    }
    
    _emissionRate = _totalParticles / _life;
    resetSystem();
}

void ParticleSystemQuad::setupVBOandVAO()
{
    // clean VAO
    glDeleteBuffers(2, &_buffersVBO[0]);
    glDeleteVertexArrays(1, &_VAOname);
    GL::bindVAO(0);
    
    glGenVertexArrays(1, &_VAOname);
    GL::bindVAO(_VAOname);

#define kQuadSize sizeof(_quads[0].bl)

    glGenBuffers(2, &_buffersVBO[0]);

    glBindBuffer(GL_ARRAY_BUFFER, _buffersVBO[0]);
    glBufferData(GL_ARRAY_BUFFER, sizeof(_quads[0]) * _totalParticles, _quads, GL_DYNAMIC_DRAW);

    // vertices
    glEnableVertexAttribArray(GLProgram::VERTEX_ATTRIB_POSITION);
    glVertexAttribPointer(GLProgram::VERTEX_ATTRIB_POSITION, 2, GL_FLOAT, GL_FALSE, kQuadSize, (GLvoid*) offsetof( V3F_C4B_T2F, vertices));

    // colors
    glEnableVertexAttribArray(GLProgram::VERTEX_ATTRIB_COLOR);
    glVertexAttribPointer(GLProgram::VERTEX_ATTRIB_COLOR, 4, GL_UNSIGNED_BYTE, GL_TRUE, kQuadSize, (GLvoid*) offsetof( V3F_C4B_T2F, colors));

    // tex coords
    glEnableVertexAttribArray(GLProgram::VERTEX_ATTRIB_TEX_COORDS);
    glVertexAttribPointer(GLProgram::VERTEX_ATTRIB_TEX_COORDS, 2, GL_FLOAT, GL_FALSE, kQuadSize, (GLvoid*) offsetof( V3F_C4B_T2F, texCoords));

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, _buffersVBO[1]);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(_indices[0]) * _totalParticles * 6, _indices, GL_STATIC_DRAW);

    // Must unbind the VAO before changing the element buffer.
    GL::bindVAO(0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    CHECK_GL_ERROR_DEBUG();
}

void ParticleSystemQuad::setupVBO()
{
    glDeleteBuffers(2, &_buffersVBO[0]);
    
    glGenBuffers(2, &_buffersVBO[0]);

    glBindBuffer(GL_ARRAY_BUFFER, _buffersVBO[0]);
    glBufferData(GL_ARRAY_BUFFER, sizeof(_quads[0]) * _totalParticles, _quads, GL_DYNAMIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, _buffersVBO[1]);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(_indices[0]) * _totalParticles * 6, _indices, GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

    CHECK_GL_ERROR_DEBUG();
}

void ParticleSystemQuad::listenBackToForeground(EventCustom* event)
{
    if (Configuration::getInstance()->supportsShareableVAO())
    {
        setupVBOandVAO();
    }
    else
    {
        setupVBO();
    }
}

bool ParticleSystemQuad::allocMemory()
{
    CCASSERT( ( !_quads && !_indices), "Memory already alloced");
    CCASSERT( !_batchNode, "Memory should not be alloced when not using batchNode");

    CC_SAFE_FREE(_quads);
    CC_SAFE_FREE(_indices);

    _quads = (V3F_C4B_T2F_Quad*)malloc(_totalParticles * sizeof(V3F_C4B_T2F_Quad));
    _indices = (GLushort*)malloc(_totalParticles * 6 * sizeof(GLushort));
    
    if( !_quads || !_indices) 
    {
        CCLOG("cocos2d: Particle system: not enough memory");
        CC_SAFE_FREE(_quads);
        CC_SAFE_FREE(_indices);

        return false;
    }

    memset(_quads, 0, _totalParticles * sizeof(V3F_C4B_T2F_Quad));
    memset(_indices, 0, _totalParticles * 6 * sizeof(GLushort));

    return true;
}

void ParticleSystemQuad::setBatchNode(ParticleBatchNode * batchNode)
{
    if( _batchNode != batchNode ) 
    {
        ParticleBatchNode* oldBatch = _batchNode;

        ParticleSystem::setBatchNode(batchNode);

        // NEW: is self render ?
        if( ! batchNode ) 
        {
            allocMemory();
            initIndices();
            setTexture(oldBatch->getTexture());
            if (Configuration::getInstance()->supportsShareableVAO())
            {
                setupVBOandVAO();
            }
            else
            {
                setupVBO();
            }
        }
        // OLD: was it self render ? cleanup
        else if( !oldBatch )
        {
            // copy current state to batch
            V3F_C4B_T2F_Quad *batchQuads = _batchNode->getTextureAtlas()->getQuads();
            V3F_C4B_T2F_Quad *quad = &(batchQuads[_atlasIndex] );
            memcpy( quad, _quads, _totalParticles * sizeof(_quads[0]) );

            CC_SAFE_FREE(_quads);
            CC_SAFE_FREE(_indices);

            glDeleteBuffers(2, &_buffersVBO[0]);
            memset(_buffersVBO, 0, sizeof(_buffersVBO));
            if (Configuration::getInstance()->supportsShareableVAO())
            {
                glDeleteVertexArrays(1, &_VAOname);
                GL::bindVAO(0);
                _VAOname = 0;
            }
        }
    }
}

ParticleSystemQuad * ParticleSystemQuad::create() {
    ParticleSystemQuad *particleSystemQuad = new ParticleSystemQuad();
    if (particleSystemQuad && particleSystemQuad->init())
    {
        particleSystemQuad->autorelease();
        return particleSystemQuad;
    }
    CC_SAFE_DELETE(particleSystemQuad);
    return nullptr;
}

std::string ParticleSystemQuad::getDescription() const
{
    return StringUtils::format("<ParticleSystemQuad | Tag = %d, Total Particles = %d>", _tag, _totalParticles);
}

NS_CC_END
