#include "Dome.h"

sgct::Engine * gEngine;

void draw();
void initGL();
void preSync();
void encode();
void decode();
void keyCallback(int key, int action);

void drawGeoCorrPatt();
void drawColCorrPatt();
void drawCube();
void loadData();
void drawTexturedObject();

Dome * mDome;

sgct::SharedShort displayState(0);
sgct::SharedBool showGeoCorrectionPattern(true);
sgct::SharedBool showBlendZones(false);
sgct::SharedBool showChannelZones(false);

const short lastState = 7;
bool useShader = true;

int main( int argc, char* argv[] )
{

	// Allocate
	gEngine = new sgct::Engine( argc, argv );

	// Bind your functions
	gEngine->setDrawFunction( draw );
	gEngine->setInitOGLFunction( initGL );
	gEngine->setPreSyncFunction( preSync );
	gEngine->setKeyboardCallbackFunction( keyCallback );
	sgct::SharedData::instance()->setEncodeFunction(encode);
	sgct::SharedData::instance()->setDecodeFunction(decode);

	// Init the engine
	if( !gEngine->init() )
	{
		delete gEngine;
		return EXIT_FAILURE;
	}

	// Main loop
	gEngine->render();

	// Clean up (de-allocate)
	delete gEngine;

	// Exit program
	exit( EXIT_SUCCESS );
}

void draw()
{
	switch( displayState.getVal() )
	{
	case 0:
	default:
		drawGeoCorrPatt();
		break;

	case 1:
		drawColCorrPatt();
		break;

	case 2:
		drawColCorrPatt();
		break;

	case 3:
		drawColCorrPatt();
		break;

	case 4:
		drawColCorrPatt();
		break;

	case 5:
		drawColCorrPatt();
		break;

	case 6:
		drawCube();
		break;

	case 7:
		drawTexturedObject();
		break;
	}
}

void initGL()
{
	mDome = new Dome(7.4f, 26.7f, FISHEYE);
	mDome->generateDisplayList();
	
	glDisable(GL_LIGHTING);
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_COLOR_MATERIAL);
	glEnable(GL_NORMALIZE);
}

void preSync()
{
	//set the time only on the master
	if( gEngine->isMaster() )
	{
		;
	}
}

void encode()
{
	sgct::SharedData::instance()->writeShort( &displayState );
	sgct::SharedData::instance()->writeBool( &showGeoCorrectionPattern );
	sgct::SharedData::instance()->writeBool( &showBlendZones );
	sgct::SharedData::instance()->writeBool( &showChannelZones );
}

void decode()
{
	sgct::SharedData::instance()->readShort( &displayState );
	sgct::SharedData::instance()->readBool( &showGeoCorrectionPattern );
	sgct::SharedData::instance()->readBool( &showBlendZones );
	sgct::SharedData::instance()->readBool( &showChannelZones );
}

void keyCallback(int key, int action)
{
	if( gEngine->isMaster() )
	{
		switch( key )
		{
		case SGCT_KEY_LEFT:
			if(action == SGCT_PRESS)
            {
				if( displayState.getVal() > 0 )
					displayState.setVal(displayState.getVal() - 1);
				else
					displayState.setVal( lastState );
            }
			break;

		case SGCT_KEY_RIGHT:
			if(action == SGCT_PRESS)
			{
				if( displayState.getVal() < lastState )
					displayState.setVal(displayState.getVal() + 1);
				else
					displayState.setVal(0);
			}
			break;

		case SGCT_KEY_B:
			if(action == SGCT_PRESS)
				showBlendZones.toggle();
			break;

		case SGCT_KEY_C:
			if(action == SGCT_PRESS)
				showChannelZones.toggle();
			break;

		case SGCT_KEY_G:
			if(action == SGCT_PRESS)
				showGeoCorrectionPattern.toggle();
			break;
		}
	}
}

void drawGeoCorrPatt()
{
	glDepthMask(GL_FALSE);

	if( showGeoCorrectionPattern.getVal() )
		mDome->drawGeoCorrPattern();

	if( showBlendZones.getVal() )
		mDome->drawBlendZones();

	if( showChannelZones.getVal() )
		mDome->drawChannelZones();

	glDepthMask(GL_TRUE);
}

void drawColCorrPatt()
{
	/*glDepthMask(GL_FALSE);
	mDome->drawColCorrPattern(colors[mSharedData->colorState].col,
		   static_cast<int>((displayState.getVal()-(DOME+1)))%5);

	glDepthMask(GL_TRUE);*/
}

void drawCube()
{
	/*vpr::System::usleep( mSharedData->delay );
	
	glPushMatrix();
	glTranslatef(offset[0],offset[1],offset[2]);

	if( mSharedData->spinningCube )
	{
		static float angle = 0.0f;
		angle += static_cast<float>(mSharedData->dt * mSharedData->speed);
		glRotatef( angle, 0.0f, 1.0f, 0.0f );
	}

	glDepthMask(GL_FALSE);

	if( mSharedData->bufferTest )
	{
		static unsigned int frames = 0;
		
		if( frames % 2 == 0 )
			glColor4f(1.0f, 0.0f, 0.0f, 1.0);
		else
			glColor4f(0.0f, 1.0f, 0.0f, 1.0);

		if( frames > 10000 )
			frames = 0;
		else
			frames++;
	}
	else
		glColor4fv(colors[mSharedData->colorState].col);
	glLineWidth(2);

	float size;
	switch(calibrationType)
	{
	case CYLINDER:
		size = 5.0f;
		break;

	case DOME:
		size = mDome->getRadius() * 2.0f;
		break;

	default:
		size = 5.0f;
		break;
	}

	glBegin(GL_LINES);
		//X-lines
		
		glVertex3f( -size*0.5f, size*0.5f, size*0.5f );
		glVertex3f( size*0.5f, size*0.5f, size*0.5f );

		glVertex3f( -size*0.5f, size*0.5f, -size*0.5f );
		glVertex3f( size*0.5f, size*0.5f, -size*0.5f );

		glVertex3f( -size*0.5f, -size*0.5f, -size*0.5f );
		glVertex3f( size*0.5f, -size*0.5f, -size*0.5f );

		glVertex3f( -size*0.5f, -size*0.5f, size*0.5f );
		glVertex3f( size*0.5f, -size*0.5f, size*0.5f );

		//Y-lines
	    glVertex3f( -size*0.5f, -size*0.5f, -size*0.5f );
		glVertex3f( -size*0.5f, size*0.5f, -size*0.5f );

		glVertex3f( size*0.5f, -size*0.5f, -size*0.5f );
		glVertex3f( size*0.5f, size*0.5f, -size*0.5f );

		glVertex3f( -size*0.5f, -size*0.5f, size*0.5f );
		glVertex3f( -size*0.5f, size*0.5f, size*0.5f );

		glVertex3f( size*0.5f, -size*0.5f, size*0.5f );
		glVertex3f( size*0.5f, size*0.5f, size*0.5f );

		//Z-lines
		glVertex3f( size*0.5f, size*0.5f, -size*0.5f );
		glVertex3f( size*0.5f, size*0.5f, size*0.5f );

		glVertex3f( -size*0.5f, size*0.5f, -size*0.5f );
		glVertex3f( -size*0.5f, size*0.5f, size*0.5f );

		glVertex3f( size*0.5f, -size*0.5f, -size*0.5f );
		glVertex3f( size*0.5f, -size*0.5f, size*0.5f );

		glVertex3f( -size*0.5f, -size*0.5f, -size*0.5f );
		glVertex3f( -size*0.5f, -size*0.5f, size*0.5f );
	glEnd();

	glDepthMask(GL_TRUE);
	glPopMatrix();

	glColor4f(1.0f, 1.0f, 1.0f, 0.3f);
	freetype::print(font, 400, 400, "FPS: %.3f\nSpeed: %.0fx\nDelay: %d us", fps, mSharedData->speed, (unsigned int)mSharedData->delay);

	*/
}

void loadData()
{
	/*//todo: fixa kompression fr�n xml
	for(unsigned int i=0;i<texFilenames.size();i++)
	  tex.loadTexure(texFilenames[i].c_str(), true, false);

	blender = Shader::create();
    const char * shaderName = "textureblend";
    useShader = blender->set( shaderName );

    if( useShader )
	{
		if ( blender->p == 0 )
			fprintf( stderr, "Shader object '%s' not created!\n", shaderName);
		else
		{
			texLoc1 = -1;
			texLoc2 = -1;
			blenderLoc = -1;
			
			texLoc1 = glGetUniformLocation( blender->p, "tex1" );
			texLoc2 = glGetUniformLocation( blender->p, "tex2" );
			blenderLoc = glGetUniformLocation( blender->p, "blender" );
			
			fprintf( stderr, "Shader object '%s' created\n", shaderName);
		}
	 }*/
}

void drawTexturedObject()
{
	/*if(tex.getStoredTextureCount() == 0)
		return;
	
	if( useShader )
    {
		float crossFadeTime = 0.6f;
		float blendVal = (mSharedData->animationTime - (10.0f-crossFadeTime))/(10.0f - (10.0f-crossFadeTime));

	    if( blendVal < 0.0f)
			blendVal = 0.0f;
	   
	    glUseProgram( blender->p );
		glUniform1i( texLoc1, 0 );
		glUniform1i( texLoc2, 1 );
		glUniform1f( blenderLoc, blendVal );
    }
	
	glActiveTexture( GL_TEXTURE0 );
	glBindTexture( GL_TEXTURE_2D, tex.getTexID(mSharedData->frameCount) );
	glEnable( GL_TEXTURE_2D );

	glActiveTexture( GL_TEXTURE1 );
	glBindTexture( GL_TEXTURE_2D, tex.getTexID((mSharedData->frameCount+1)%tex.getStoredTextureCount()) );
	glEnable( GL_TEXTURE_2D );
	
	if( calibrationType == DOME )
	{
		mDome->setTiltOffset( mSharedData->tiltOffset );
		mDome->drawTexturedSphere();
	}

	glActiveTexture( GL_TEXTURE1 );
	glDisable( GL_TEXTURE_2D );
	glActiveTexture( GL_TEXTURE0 );
	glDisable( GL_TEXTURE_2D );	

	if( useShader )
	   glUseProgram( 0 );

	*/
}
