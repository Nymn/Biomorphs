#include "biomorphs.h"
#include "core\random.h"
#include "core\profiler.h"

#include <ctime>

Biomorphs::Biomorphs( void* userData )
	: m_appConfig(*((D3DAppConfig*)userData))
{
}

Biomorphs::~Biomorphs()
{
}

void Biomorphs::_resetDNA()
{
	Random::seed( (int)time(NULL) );

	// initialise dna values to a random tree-ish start point
	D3DXVECTOR3 baseColour( Random::getFloat( 0.5f, 1.0f ),
							Random::getFloat( 0.5f, 1.0f ),
							Random::getFloat( 0.5f, 1.0f ) );

	D3DXVECTOR3 colourMod( Random::getFloat( 0.6f, 1.4f ),
							Random::getFloat( 0.6f, 1.4f ),
							Random::getFloat( 0.6f, 1.4f ) );

	m_testDNA = MAKEDNA(	Random::getInt(4, 8), 
							Random::getFloat(10.0f,120.0f), 
							Random::getFloat(0.8f,1.0f), 
							Random::getFloat( 0.8f, 1.2f ), 
							Random::getFloat( 0.7f, 1.3f ),
							baseColour,
							colourMod);

	m_generation = 0;

	mBiomorphManager.GenerateBiomorph( m_testDNA );

	if( mMorphInstance.IsValid() )
	{
		mBiomorphManager.DestroyInstance( mMorphInstance );
	}
	mMorphInstance = mBiomorphManager.CreateInstance( m_testDNA );
}

void Biomorphs::_drawOverlay()
{
	char textOut[256] = {'\0'};
	Font::DrawParameters dp;
	const float textColour[] = {1.0f,1.0f,1.0f,1.0f};
	Vector2 textPos( 16, m_appConfig.m_windowHeight - 370 );
	memcpy( dp.mColour, textColour, sizeof(textColour) );

	// render profiler data
	dp.mJustification = Font::DRAW_LEFT;
	PROFILER_ITERATE_DATA(ItName)
	{
		textPos.x() = 16 + ((*ItName).mStackLevel * 16);
		sprintf_s(textOut, "%s: %3.3fms\n", (*ItName).mName.c_str(), (*ItName).mTimeDiff * 1000.0f);
		m_device.DrawText( textOut, m_font, dp, textPos );	textPos.y() = textPos.y() + 18;
	}

	textPos.x() = 16;
	textPos.y() = textPos.y() + 30;
	sprintf_s(textOut, "Generation: %d", m_generation);
	m_device.DrawText( textOut, m_font, dp, textPos );

	textPos.y() = textPos.y() + 18;
	sprintf_s(textOut, "DNA: %x%x%x%x", m_testDNA.mFullSequenceHigh0, m_testDNA.mFullSequenceLow0
										  , m_testDNA.mFullSequenceHigh1, m_testDNA.mFullSequenceLow1);
	m_device.DrawText( textOut, m_font, dp, textPos );
}

void Biomorphs::_drawMorphToScreen()
{
	SCOPED_PROFILE(RenderMorphToScreen);

	if( !mMorphInstance.IsValid() )
	{
		return;
	}

	float aspect = (float)m_appConfig.m_windowWidth / (float)m_appConfig.m_windowHeight;

	// switch back to rendering to back buffer
	Rendertarget& backBuffer = m_device.GetBackBuffer();
	DepthStencilBuffer& depthBuffer = m_device.GetDepthStencilBuffer();
	m_device.SetRenderTargets( &backBuffer, &depthBuffer );

	// Set the viewport
	Viewport vp;
	vp.topLeft = Vector2(0,0);
	vp.depthRange = Vector2f(0.0f,1.0f);
	vp.dimensions = Vector2(m_appConfig.m_windowWidth, m_appConfig.m_windowHeight);
	m_device.SetViewport( vp );

	// Clear buffers
	static float clearColour[4] = {0.05f, 0.15f, 0.25f, 1.0f};
	m_device.ClearTarget( backBuffer, clearColour );
	m_device.ClearTarget( depthBuffer, 1.0f, 0 );

	// draw the biomorph as a sprite
	m_spriteRender.GetTexture() = *mMorphInstance.GetTexture();
		 
	m_spriteRender.RemoveSprites();
	m_spriteRender.AddSprite( 0, D3DXVECTOR2(-0.7f,-0.7f), D3DXVECTOR2(1.4f,1.4f) );
	float scale = 0.6f;
	m_spriteRender.Draw( m_device, D3DXVECTOR2(0.0f,0.0f), D3DXVECTOR2(scale,scale*aspect), "Render" );
}

void Biomorphs::_render(Timer& timer)
{
	{
		SCOPED_PROFILE(RenderAll);
		
		// draw the morph on screen
		_drawMorphToScreen();
	
		//now render the bloom from the backbuffer
		static BloomRender::DrawParameters dp( 0.15f, 1.0f,
												0.2f, 1.0f,
												0.8f, 1.0f,
												0.4f, 1.0f );
		m_bloom.Render(dp);
	}

	// display overlay
	_drawOverlay();

	m_device.PresentBackbuffer();
}

bool Biomorphs::_initialise()
{
	// load the sprite shader
	Effect::Parameters ep("shaders//textured_sprite.fx");
	m_spriteShader = m_device.CreateEffect(ep);

	// create the morph renderer
	BiomorphManager::Parameters biop;
	biop.TextureSize = 512;
	mBiomorphManager.Initialise( &m_device, biop );

	// Create a sprite renderer
	SpriteRender::Parameters sp;
	sp.mMaxSprites = 1024 * 8;
	sp.shader = m_spriteShader;
	m_spriteRender.Create( m_device, sp );

	// Create bloom renderer
	BloomRender::Parameters bp;
	bp.mWidth = m_appConfig.m_windowWidth;
	bp.mHeight = m_appConfig.m_windowHeight;
	m_bloom.Create( &m_device, bp );

	// Reset DNA and generate biomorph instance
	_resetDNA();

	return true;
}

bool Biomorphs::_update(Timer& timer)
{
	PROFILER_RESET();
	SCOPED_PROFILE(AppUpdate);

	if( m_inputModule->keyPressed( VK_SPACE ) )
	{
		_resetDNA();
	}
	else
	{
		MutateDNA( m_testDNA );
		mBiomorphManager.GenerateBiomorph( m_testDNA );

		if( mMorphInstance.IsValid() )
		{
			mBiomorphManager.DestroyInstance( mMorphInstance );
		}
		mMorphInstance = mBiomorphManager.CreateInstance( m_testDNA );

		m_generation++;
	}

	mBiomorphManager.CleanupDatabase();

	return true;
}

bool Biomorphs::update( Timer& timer )
{
	_update(timer);
	_render(timer);

	return true;
}

bool Biomorphs::shutdown()
{
	PROFILER_CLEANUP();

	m_device.Flush();

	m_bloom.Release();

	m_device.Release(m_spriteRender.GetTexture());

	m_device.Release( m_font );

	mBiomorphManager.DestroyInstance( mMorphInstance );
	mBiomorphManager.Release();

	m_device.Shutdown();
	return true;
}

bool Biomorphs::connect( IModuleConnector& connector )
{
	m_inputModule = (InputModule*)connector.getModule( "Input" );
	return true;
}

bool Biomorphs::startup()
{
	if( !initDevice() )
		return false;

	// Create a font
	Font::Parameters fontParams;
	fontParams.mSize = 16;
	fontParams.mWeight = 400;
	strcpy_s(fontParams.mTypeface, "Arial");
	m_font = m_device.CreateFont( fontParams );

	return _initialise();
}

bool Biomorphs::initDevice()
{
	// Init the device
	Device::InitParameters params;
#ifdef _DEBUG
	params.enableDebugD3d = true;
#else
	params.enableDebugD3d = false;
#endif
	params.msaaCount = 1;
	params.msaaQuality = 0;
	params.windowHeight = m_appConfig.m_windowHeight;
	params.windowWidth = m_appConfig.m_windowWidth;
	if(!m_device.Initialise(params))
	{
		return false;
	}

	return true;
}