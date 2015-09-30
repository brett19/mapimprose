#include <string>
#include <vector>
#include <boost/algorithm/string.hpp>
#include "Max.h"
#include <iparamm2.h>
#include <toneop.h>
#include "istdplug.h"
#include "stdmat.h"
#include "decomp.h"
#include "shape.h"
#include "splshape.h"
#include "dummy.h"
#include "bmmlib.h"
#include "bitmap.h"
#include "MESHDLIB.H"
#include <INodeBakeProperties.h> 
#include "haFile.hpp"
#include "ObjectList.hpp"
#include "MapBlock.hpp"
#include "HeightMap.hpp"
#include "TileMap.hpp"
#include "MapData.hpp"
#include "roseDataTable.hpp"

HINSTANCE hInstance;

enum VertexFormats
{
	//RMVF_INVALID = 1,
	RMVF_POSITION = 2,
	RMVF_NORMALS = 4,
	RMVF_COLORS = 8,
	RMVF_BONEINDICES = 16,
	RMVF_BONEWEIGHTS = 32,
	RMVF_TANGENTS = 64,
	RMVF_UVMAP1 = 128,
	RMVF_UVMAP2 = 256,
	RMVF_UVMAP3 = 512,
	RMVF_UVMAP4 = 1024
};

class MapImport
	: public SceneImport
{
public:
	MapImport( )
	{
	};

	~MapImport( )
	{
	};

	int ExtCount( )
	{
		return 2;
	};

	const TCHAR * Ext( int n )
	{
		if( n == 0 ) return _T("ZON");
		if( n == 1 ) return _T("IFO");
		return _T("");
	};

	const TCHAR * LongDesc( )
	{
		return _T("ROSE Map Importer");
	};

	const TCHAR * ShortDesc( )
	{
		return _T("ROSE Map Importer");
	};

	const TCHAR * AuthorName( )
	{
		return _T("Brett19 (TitanROSE)");
	};

	const TCHAR * CopyrightMessage( )
	{
		return _T("Copyright Brett19 2008-Present");
	};

	const TCHAR * OtherMessage1( )
	{
		return _T("");
	};

	const TCHAR * OtherMessage2( )
	{
		return _T("");
	};

	unsigned int Version( )
	{
		return 201;
	};

	void ShowAbout( HWND hWnd )
	{
	};

	void SetBakeParams( INode* node, int mapChannel = 0 )
	{
		INodeBakeProperties* pBake = static_cast<INodeBakeProperties*>(node->GetInterface(NODE_BAKE_PROPERTIES_INTERFACE));
		if( mapChannel > 0 ) {
			pBake->SetBakeEnabled( true );
			pBake->SetBakeMapChannel( mapChannel );
		} else {
			pBake->SetBakeEnabled( false );
		}
	}

	int DoImport( const TCHAR* name, ImpInterface* i,Interface* gi, BOOL suppressPrompts=FALSE )
	{
		char tmpInfo[256];
		Matrix3 mtrx;

		DebugPrint( "--- STARTED IMPORT ---" );

		DebugPrint( "Disabling Viewport..." );
		gi->DisableSceneRedraw( );
		gi->EnableUndo( false );

		DebugPrint( "Importing..." );

		char fullPath[512], drive[32], dir[512], fname[128], ext[32];
		int selSecX = -1, selSecY = -1;
		char zonPath[512];

		_fullpath( fullPath, name, 512 );

		int fullPathLen = (int)strlen(fullPath);
		if( strcmpi(&name[fullPathLen-3],"IFO") == 0 ) {
			_splitpath( fullPath, drive, dir, fname, ext );

			std::vector<std::string> splitDir;
			boost::split( splitDir, dir, boost::is_any_of("\\/") );
			splitDir.pop_back();

			sscanf( fname, "%2d_%2d.IFO", &selSecX, &selSecY );

			std::string mapDirName = splitDir.back();
			sprintf( zonPath, "%s%s%s.ZON", drive, dir, mapDirName.c_str() );

		} else if( strcmpi(&name[fullPathLen-3],"ZON") == 0 ) {
			strcpy( zonPath, fullPath );
		} else {
			return 0;
		}

		DebugPrint( "File Path:" );
		DebugPrint( name );
		
		DebugPrint( "Full Path:" );
		DebugPrint( fullPath );
		
		DebugPrint( "ZON Path:" );
		DebugPrint( zonPath );
		
		DebugPrint( "Sector: (%d,%d)", selSecX, selSecY );

		_splitpath( fullPath, drive, dir, fname, ext );

		std::string dirPath = std::string(drive) + std::string(dir);
		std::string mapDir = dirPath;
		DebugPrint( "  - ZON Directory:" );
		DebugPrint( dirPath.c_str() );

		std::vector<std::string> splitPath;
		boost::split( splitPath, dirPath, boost::is_any_of("\\/") );

		splitPath.pop_back( );
		std::string mapName = splitPath.back( );
		splitPath.pop_back( );
		std::string planetName = splitPath.back( );
		splitPath.pop_back( );
		splitPath.pop_back( );
		splitPath.pop_back( );
		std::string basePath = boost::join( splitPath, "\\" ) + "\\";


		DebugPrint( "  - Map Name:" );
		DebugPrint( mapName.c_str() );
		DebugPrint( "  - Planet Name:" );
		DebugPrint( planetName.c_str() );
		DebugPrint( "  - Base Directory:" );
		DebugPrint( basePath.c_str() );

		std::string stbPath = basePath + "3DDATA\\STB\\LIST_ZONE.STB";
		DebugPrint( "  - Zone List Path:" );
		DebugPrint( stbPath.c_str() );

		roseDataTable zoneList;
		zoneList.Load( (const haString)stbPath.c_str() );

		int mapRecordId = 0;
		for( int i = 1; i < zoneList.RecordCount(); ++i )
		{
			haChar zonePath[ 256 ];
			zoneList.ReadString( i, 1, zonePath );

			_splitpath( zonePath, drive, dir, fname, ext );
			if( strcmp(fname,"") == 0 ) continue;

			if( fname == mapName ) {
				mapRecordId = i;
				break;
			}
		}

		if( mapRecordId == 0 ) {
			return 1;
		}

		int startX = zoneList.ReadInt( mapRecordId, 9 );
		int startY = zoneList.ReadInt( mapRecordId, 10 );

		char cnstPath[512], decoPath[512];
		zoneList.ReadString( mapRecordId, 11, decoPath );
		zoneList.ReadString( mapRecordId, 12, cnstPath );

		int endX = startX;
		int endY = startY;

		for( int ix = startX + 1; ix < 65; ix++ ) {
			sprintf( tmpInfo, "%s%d_%d.HIM", mapDir.c_str(), ix, startY );
			
			DebugPrint( "  - Zone Bounds Test:" );
			DebugPrint( tmpInfo );

			FILE* fhTest = fopen( tmpInfo, "rb" );
			if( fhTest ) {
				endX = ix;
				fclose( fhTest );
			} else {
				break;
			}
		}
		for( int iy = startY + 1; iy < 65; iy++ ) {
			sprintf( tmpInfo, "%s%d_%d.HIM", mapDir.c_str(), startX, iy );

			DebugPrint( "  - Zone Bounds Test:" );
			DebugPrint( tmpInfo );

			FILE* fhTest = fopen( tmpInfo, "rb" );
			if( fhTest ) {
				endY = iy;
				fclose( fhTest );
			} else {
				break;
			}
		}
		
		if( selSecX >= 0 && selSecY >= 0 ) {
			startX = endX = selSecX;
			startY = endY = selSecY;
		}

		sprintf( tmpInfo, "%d,%d -> %d,%d", startX, startY, endX, endY );
		DebugPrint( "  - Zone Bounds Pos:" );
		DebugPrint( tmpInfo );

		DebugPrint( "  - Zone Object List Paths:" );
		DebugPrint( decoPath );
		DebugPrint( cnstPath );

		short minx = startX;
		short miny = startY;
		short maxx = endX;
		short maxy = endY;

		char mappath[ 256 ], zonpath[ 256 ], decopath[ 256 ], cnstpath[ 256 ], mapnodename[ 256 ];
		sprintf( mappath, "%s", mapDir.c_str() );
		DebugPrint( "Generated Map Path : %s", mappath );
		sprintf( zonpath, "%s%s.ZON", mappath, mapName.c_str() );
		DebugPrint( "Generated ZON Path : %s", zonpath );
		sprintf( decopath, "%s%s", basePath.c_str(), decoPath );
		DebugPrint( "Generated DECO Path : %s", decopath );
		sprintf( cnstpath, "%s%s", basePath.c_str(), cnstPath );
		DebugPrint( "Generated CNST Path : %s", cnstpath );

		DebugPrint( "Loading ZON" );

		MapData md;
		md.Load( zonpath );

		DebugPrint( "Done Loading ZON" );

		DebugPrint( "Generating Terrain Materials (%d)", md.combcount );

		MultiMtl* mmtl = NewDefaultMultiMtl( );
		mmtl->SetNumSubMtls( md.combcount );
		for( int i = 0; i < md.combcount; i++ )
		{
			char texpath[ 256 ];
			sprintf( texpath, "%s\\%s", basePath.c_str(), md.tiles[md.combs[i].tile1].path );
			BitmapTex* btex1 = NewDefaultBitmapTex( );
			btex1->SetMapName( texpath );
			sprintf( texpath, "%s\\%s", basePath.c_str(), md.tiles[md.combs[i].tile2].path );
			BitmapTex* btex2 = NewDefaultBitmapTex( );
			btex2->SetMapName( texpath );
			BitmapTex* btex3 = NewDefaultBitmapTex( );
			btex3->SetMapName( texpath );

			btex1->SetAlphaSource( ALPHA_NONE );
			btex2->SetAlphaSource( ALPHA_NONE );
			btex3->SetAlphaAsMono( true );
			
			if( md.combs[i].rot == 2 ) {
				btex3->GetUVGen( )->SetUAng( 0.0f, 0 );
				btex3->GetUVGen( )->SetVAng( 3.14159265f, 0 );
			} else if( md.combs[i].rot == 3 ) {
				btex3->GetUVGen( )->SetUAng( 3.14159265f, 0 );
				btex3->GetUVGen( )->SetVAng( 0.0f, 0 );
			} else if( md.combs[i].rot == 4 ) {
				btex3->GetUVGen( )->SetUAng( 3.14159265f, 0 );
				btex3->GetUVGen( )->SetVAng( 3.14159265f, 0 );
			}

			MultiTex* tex = NewDefaultMixTex( );
			tex->SetNumSubTexmaps( 3 );
			tex->SetSubTexmap( 0, btex1 );
			tex->SetSubTexmap( 1, btex2 );
			tex->SetSubTexmap( 2, btex3 );

			StdMat2* mtl3 = NewDefaultStdMat( );
			mtl3->SetSubTexmap( ID_DI, tex );
			
			mmtl->SetSubMtl( i, mtl3 );
		}

		DebugPrint( "Done Terrain Materials" );

		ObjectList decolist;
		ObjectList cnstlist;
		decolist.Load( decopath );
		cnstlist.Load( cnstpath );

		char nodename[ 256 ], ifopath[ 256 ], himpath[ 256 ], tilpath[ 256 ];
		for( int ix = minx; ix <= maxx; ix++ )
		{
			for( int iy = miny; iy <= maxy; iy++ )
			{
				sprintf( nodename, "%d_%d", ix, iy );

				sprintf( ifopath, "%s\\%d_%d.IFO", mappath, ix, iy );
				DebugPrint( "Generated Ifo Path : %s", ifopath );
				sprintf( himpath, "%s\\%d_%d.HIM", mappath, ix, iy );
				DebugPrint( "Generated Him Path : %s", himpath );
				sprintf( tilpath, "%s\\%d_%d.TIL", mappath, ix, iy );
				DebugPrint( "Generated Him Path : %s", himpath );

				LoadMap( ifopath, basePath.c_str(), nodename, decolist, cnstlist, gi );

				TriObject* terrainobj = CreateNewTriObject( );
				INode* terrainnode = gi->CreateObjectNode( terrainobj );
				sprintf( mapnodename, "%s_TERR", nodename );
				terrainnode->SetName( mapnodename );
				mtrx.IdentityMatrix( );
				Point3 p( 0.0f, 0.0f, 0.0f );
				mtrx.SetTranslate( p );
				terrainnode->SetNodeTM( 0, mtrx );
				terrainnode->SetWireColor( 0xFFCCCCCC );
				LoadHeightmap( himpath, tilpath, md, ix, iy, terrainobj );
				terrainnode->SetMtl( mmtl );

				SetBakeParams( terrainnode, 2 );


				/*
				TriObject* obj = CreateNewTriObject( );
				INode* objnode = gi->CreateObjectNode( obj );
				sprintf( objnodename, "%s_%d_%d", nodename, 0, 0 );
				objnode->SetName( objnodename );
				mtrx.IdentityMatrix( );
				objnode->SetNodeTM( 0.0f, mtrx );
				LoadMesh( "C:\\Program Files\\Triggersoft\\Rose Online\\3ddata\\JUNON\\VILLAGE\\CASTLEGATE01\\CASTLEGATE01.ZMS", obj );
				*/
			}
		}

		DebugPrint( "Done Importing!" );

		DebugPrint( "Enabling Viewport..." );
		gi->EnableUndo( true );
		gi->EnableSceneRedraw( );

		DebugPrint( "--- ENDED IMPORT ---" );

		return 1;
	};

	void LoadHeightmap( char* himpath, char* tilpath, MapData& md, short chunkx, short chunky, TriObject*& obj )
	{
		int vertid = 0;

		HeightMap hm;
		hm.Load( himpath );

		TileMap tm;
		tm.Load( tilpath );

		Mesh& msh = obj->mesh;

		msh.setMapSupport( 1 );
		msh.maps[1].setNumVerts( 16 * 16 * 5 * 5 );
		msh.setMapSupport( 2 );
		msh.maps[2].setNumVerts( 65 * 65 );

		int vert_lookup[65][65];

		msh.setNumVerts( 65 * 65 );
		vertid = 0;
		for( int ix = 0; ix < 65; ++ix ) {
			for( int iy = 0; iy < 65; ++iy ) {
				Point3 vert;
				vert.x = ((ix*2.5f) + (chunkx*160.0f));
				vert.y = 10400.0f - ((iy*2.5f) + (chunky*160.0f));
				vert.z = hm.heights[ix][iy] / 100.0f;
				vert.x -= 5200.0f;
				vert.y -= 5200.0f;
				msh.setVert( vertid, vert );

				msh.maps[2].tv[vertid].x = (float)ix / 64.0f;
				msh.maps[2].tv[vertid].y = 1.0f - ((float)iy / 64.0f);

				vert_lookup[ix][iy] = vertid;

				vertid++;
			}
		}

		int face_lookup[16][16][100];

		vertid = 0;
		for( int ix = 0; ix < 16; ix++ ) {
			for( int iy = 0; iy < 16; iy++ ) {

				int vid = 0;
				for( int vy = 0; vy < 5; vy++ ) {
					for( int vx = 0; vx < 5; vx++ ) {
						int dx4 = ix * 4 + vx;
						int dy4 = iy * 4 + vy;

						msh.maps[1].tv[vertid].x = (float)vx / 4.0f;
						msh.maps[1].tv[vertid].y = 1.0f - ((float)vy / 4.0f);

						face_lookup[ix][iy][vid] = vert_lookup[dx4][dy4];

						vertid++;
						vid++;
					}
				}
			}
		}

		short predefindices[32][3] = {
			{ 5, 0, 6 }, { 6, 0, 1 }, { 10, 5, 11 }, { 11, 5, 6 }, { 15, 10, 16 },
			{ 16, 10, 11 }, { 20, 15, 21 }, { 21, 15, 16 }, { 6, 1, 7 }, { 7, 1, 2 },
			{ 11, 6, 12 }, { 12, 6, 7 }, { 16, 11, 17 }, { 17, 11, 12 }, { 21, 16, 22 },
			{ 22, 16, 17 }, { 7, 2, 8 }, { 8, 2, 3 }, { 12, 7, 13 }, { 13, 7, 8 },
			{ 17, 12, 18 }, { 18, 12, 13 }, { 22, 17, 23 }, { 23, 17, 18 }, { 8, 3, 9 },
			{ 9, 3, 4 }, { 13, 8, 14 }, { 14, 8, 9 }, { 18, 13, 19 }, { 19, 13, 14 },
			{ 23, 18, 24 }, { 24, 18, 19 }
		};

		msh.setNumFaces( 16 * 16 * 4 * 4 * 2 );
		msh.maps[1].setNumFaces( 16 * 16 * 4 * 4 * 2 );
		msh.maps[2].setNumFaces( 16 * 16 * 4 * 4 * 2 );
		for( int ix = 0; ix < 16; ix++ )
		{
			for( int iy = 0; iy < 16; iy++ )
			{
				int i = ix*16+iy;
				for( int j = 0; j < 32; j++ )
				{
					msh.faces[i*32+j].v[0] = face_lookup[ix][iy][predefindices[j][0]];
					msh.faces[i*32+j].v[1] = face_lookup[ix][iy][predefindices[j][1]];
					msh.faces[i*32+j].v[2] = face_lookup[ix][iy][predefindices[j][2]];

					msh.maps[1].tf[i*32+j].t[0] = i*25 + predefindices[j][0];
					msh.maps[1].tf[i*32+j].t[1] = i*25 + predefindices[j][1];
					msh.maps[1].tf[i*32+j].t[2] = i*25 + predefindices[j][2];

					msh.maps[2].tf[i*32+j].t[0] = face_lookup[ix][iy][predefindices[j][0]];
					msh.maps[2].tf[i*32+j].t[1] = face_lookup[ix][iy][predefindices[j][1]];
					msh.maps[2].tf[i*32+j].t[2] = face_lookup[ix][iy][predefindices[j][2]];

					msh.faces[i*32+j].setMatID( tm.tiles[ix][iy].tileid );
				}
			}
		}

		// 32 Faces * 3 Verts/Face
		for( int i = 0; i < 16 * 16 * 4 * 4 * 2; i++ )
			msh.FlipNormal( i );
		
		msh.InvalidateTopologyCache( );
		msh.InvalidateGeomCache( );
		msh.InvalidateEdgeList( );
		msh.buildBoundingBox( );
		msh.buildNormals( );

		msh.AutoSmooth( 3.1415, false );


	};

	void LoadMap( const char* ifopath, const char* basepath, const char* blockname, ObjectList& decolist, ObjectList& cnstlist, Interface*& gi )
	{
		Matrix3 tmpm;
		MapBlock ifo;
		ifo.Load( ifopath );

		DebugPrint( "Loading %d water", ifo.watercount );
		for( int i = 0; i < ifo.watercount; i++ )
		{
			DebugPrint( "|- Creating node" );
			TriObject* obj = CreateNewTriObject( );
			INode* objnode = gi->CreateObjectNode( obj );
			char objnodename[ 256 ];
			sprintf( objnodename, "%s_WATER_%d", blockname, i );
			objnode->SetName( objnodename );
			Matrix3 tmp;
			tmp.IdentityMatrix( );
			objnode->SetNodeTM( 0, tmp );
			objnode->SetWireColor( 0xFF0000FF );

			DebugPrint( "|- Generating Material" );

			StdMat2* mtl = NewDefaultStdMat( );
			mtl->SetDiffuse( Color(0.8f, 0.8f, 1.0f), 0 );
			mtl->SetOpacity( 0.6f, 0 );
			objnode->SetMtl( mtl );

			DebugPrint( "|- Generating Mesh data" );
			Mesh& msh = obj->mesh;
			msh.setNumVerts( 4 );
			msh.verts[0].x = ifo.waters[i].p1.x;
			msh.verts[0].y = ifo.waters[i].p1.y;
			msh.verts[0].z = ifo.waters[i].p1.z;
			msh.verts[1].x = ifo.waters[i].p1.x;
			msh.verts[1].y = ifo.waters[i].p2.y;
			msh.verts[1].z = ifo.waters[i].p2.z;
			msh.verts[2].x = ifo.waters[i].p2.x;
			msh.verts[2].y = ifo.waters[i].p1.y;
			msh.verts[2].z = ifo.waters[i].p1.z;
			msh.verts[3].x = ifo.waters[i].p2.x;
			msh.verts[3].y = ifo.waters[i].p2.y;
			msh.verts[3].z = ifo.waters[i].p2.z;
			msh.setNumFaces( 2 );
			msh.faces[0].v[0] = 0;
			msh.faces[0].v[1] = 1;
			msh.faces[0].v[2] = 2;
			msh.faces[1].v[0] = 3;
			msh.faces[1].v[1] = 2;
			msh.faces[1].v[2] = 1;

			DebugPrint( "|- Update max" );
			msh.InvalidateTopologyCache( );
			msh.InvalidateGeomCache( );
			msh.InvalidateEdgeList( );
			msh.buildBoundingBox( );
			msh.buildNormals( );

			SetBakeParams( objnode, 0 );

			DebugPrint( "|- Done!" );
		};

		DebugPrint( "Loading %d CNST objects", ifo.cnstcount );
		///*
		for( int i = 0; i < ifo.cnstcount; i++ )
		{
			MapBlock::CnstEntry ifoe = ifo.cnsts[i];

			if( ifoe.meshid >= cnstlist.objcount ) {
				DebugPrint( "Skipped loading object due to out-of-bounds error (%i).", ifoe.meshid );
				continue;
			}

			ObjectList::Object& obj = cnstlist.objects[ifoe.meshid];
			
			Matrix3 ifom;
			ifom.IdentityMatrix( );

			tmpm.IdentityMatrix( );
			tmpm.SetRotate( ifoe.r );
			tmpm.Invert( );
			ifom *= tmpm;
			tmpm.IdentityMatrix( );
			tmpm.SetScale( ifoe.s );
			ifom *= tmpm;
			tmpm.IdentityMatrix( );
			tmpm.SetTrans( ifoe.p/100 );
			ifom *= tmpm;

			for( int j = 0; j < obj.partcount; j++ )
			{
				ObjectList::ObjectPart part = obj.parts[j];

				Matrix3 partm;
				partm.IdentityMatrix( );

				tmpm.IdentityMatrix( );
				tmpm.SetRotate( part.r );
				partm *= tmpm;
				tmpm.IdentityMatrix( );
				tmpm.SetScale( part.s );
				partm *= tmpm;
				tmpm.IdentityMatrix( );
				tmpm.SetTrans( part.p );
				partm *= tmpm;
				partm *= ifom;

				TriObject* obj = CreateNewTriObject( );
				INode* objnode = gi->CreateObjectNode( obj );
				char objnodename[ 256 ];
				sprintf( objnodename, "%s_CNST_%d_%d", blockname, i, j );
				objnode->SetName( objnodename );
				objnode->SetNodeTM( 0, partm );
				objnode->SetWireColor( 0xFFFF0000 );

				char texpath[ 256 ];
				sprintf( texpath, "%s\\%s", basepath, cnstlist.mats[part.texid].path );
				BitmapTex* btex = NewDefaultBitmapTex( );
				btex->SetMapName( texpath );
				StdMat2* mtl = NewDefaultStdMat( );
				mtl->SetSubTexmap( ID_DI, btex );
				if( cnstlist.mats[part.texid].alphaenabled )
				{
					BitmapTex* btex2 = NewDefaultBitmapTex( );
					btex2->SetMapName( texpath );
					btex2->SetAlphaAsMono( true );
					mtl->SetSubTexmap( ID_OP, btex2 );
				}
				if( cnstlist.mats[part.texid].twosided )
				{
					mtl->SetTwoSided( true );
				}
				objnode->SetMtl( mtl );

				char meshpath[ 256 ];
				sprintf( meshpath, "%s\\%s", basepath, cnstlist.meshs[part.meshid].path );
				LoadMesh( meshpath, obj );

				SetBakeParams( objnode, 2 );
			};
		};
		//*/

		DebugPrint( "Loading %d DECO objects", ifo.decocount );
		///*
		for( int i = 0; i < ifo.decocount; i++ )
		{
			MapBlock::DecoEntry ifoe = ifo.decos[i];

			if( ifoe.meshid >= cnstlist.objcount ) {
				DebugPrint( "Skipped loading object due to out-of-bounds error (%i).", ifoe.meshid );
				continue;
			}

			ObjectList::Object& obj = decolist.objects[ifoe.meshid];
			
			Matrix3 ifom;
			ifom.IdentityMatrix( );

			tmpm.IdentityMatrix( );
			tmpm.SetRotate( ifoe.r );
			tmpm.Invert( );
			ifom *= tmpm;
			tmpm.IdentityMatrix( );
			tmpm.SetScale( ifoe.s );
			ifom *= tmpm;
			tmpm.IdentityMatrix( );
			tmpm.SetTrans( ifoe.p/100 );
			ifom *= tmpm;

			for( int j = 0; j < obj.partcount; j++ )
			{
				ObjectList::ObjectPart part = obj.parts[j];

				Matrix3 partm;
				partm.IdentityMatrix( );

				tmpm.IdentityMatrix( );
				tmpm.SetRotate( part.r );
				partm *= tmpm;
				tmpm.IdentityMatrix( );
				tmpm.SetScale( part.s );
				partm *= tmpm;
				tmpm.IdentityMatrix( );
				tmpm.SetTrans( part.p );
				partm *= tmpm;
				partm *= ifom;

				TriObject* obj = CreateNewTriObject( );
				INode* objnode = gi->CreateObjectNode( obj );
				char objnodename[ 256 ];
				sprintf( objnodename, "%s_DECO_%d_%d", blockname, i, j );
				objnode->SetName( objnodename );
				objnode->SetNodeTM( 0, partm );
				objnode->SetWireColor( 0xFF00FF00 );

				char texpath[ 256 ];
				sprintf( texpath, "%s\\%s", basepath, decolist.mats[part.texid].path );
				BitmapTex* btex = NewDefaultBitmapTex( );
				btex->SetMapName( texpath );
				StdMat2* mtl = NewDefaultStdMat( );
				mtl->SetSubTexmap( ID_DI, btex );
				if( decolist.mats[part.texid].alphaenabled )
				{
					BitmapTex* btex2 = NewDefaultBitmapTex( );
					btex2->SetMapName( texpath );
					btex2->SetAlphaAsMono( true );
					mtl->SetSubTexmap( ID_OP, btex2 );
				}
				if( decolist.mats[part.texid].twosided )
				{
					mtl->SetTwoSided( true );
				}
				objnode->SetMtl( mtl );

				char meshpath[ 256 ];
				sprintf( meshpath, "%s\\%s", basepath, decolist.meshs[part.meshid].path );
				LoadMesh( meshpath, obj );

				SetBakeParams( objnode, 2 );
			}
		}
		//*/
	};

	void LoadMesh( char* meshpath, TriObject*& obj )
	{
		DebugPrint( "Loading mesh '%s'", meshpath );

		Mesh& msh = obj->mesh;

		haFile fh;
		if( !fh.Open( meshpath, "rb" ) )
		{
			DebugPrint( "Failed to load mesh '%s'", meshpath );
			return;
		}

		fh.Seek( 8 ); // Format Name
		int vertformat = fh.ReadInt32( );
		fh.Seek( 24 ); // Bounding Box

		short bonelookupcount = fh.ReadInt16( );
		fh.Seek( bonelookupcount * 2 );

		short vertcount = fh.ReadInt16( );
		msh.setNumVerts( vertcount );

		if( vertformat & RMVF_POSITION )
		{
			for( int i = 0; i < vertcount; i++ )
			{
				msh.verts[i].x = fh.ReadReal32( );
				msh.verts[i].y = fh.ReadReal32( );
				msh.verts[i].z = fh.ReadReal32( );
			}
		}

		if( vertformat & RMVF_NORMALS )
		{
			for( int i = 0; i < vertcount; i++ )
			{
				Point3 p;
				p.x = fh.ReadReal32( );
				p.y = fh.ReadReal32( );
				p.z = fh.ReadReal32( );
				msh.setNormal( i, p );
			}
		}

		if( vertformat & RMVF_COLORS )
		{
			for( int i = 0; i < vertcount; i++ )
			{
				fh.ReadInt32( );
			}
		}


		for( int i = 0; i < vertcount; i++ )
		{
			if( vertformat & RMVF_BONEWEIGHTS )
			{
				fh.ReadReal32( );
				fh.ReadReal32( );
				fh.ReadReal32( );
				fh.ReadReal32( );
			};
			if( vertformat & RMVF_BONEINDICES )
			{
				fh.ReadInt16( );
				fh.ReadInt16( );
				fh.ReadInt16( );
				fh.ReadInt16( );
			}
		}

		if( vertformat & RMVF_TANGENTS )
		{
			for( int i = 0; i < vertcount; i++ )
			{
				fh.ReadReal32( );
				fh.ReadReal32( );
				fh.ReadReal32( );
			}
		}

		if( vertformat & RMVF_UVMAP1 )
		{
			msh.setMapSupport( 1 );
			msh.maps[1].setNumVerts( vertcount );
			for( int i = 0; i < vertcount; i++ )
			{
				msh.maps[1].tv[i].x = fh.ReadReal32( );
				msh.maps[1].tv[i].y = 1.0f - fh.ReadReal32( );
			}
		}
		if( vertformat & RMVF_UVMAP2 )
		{
			msh.setMapSupport( 2 );
			msh.maps[2].setNumVerts( vertcount );
			for( int i = 0; i < vertcount; i++ )
			{
				msh.maps[2].tv[i].x = fh.ReadReal32( );
				msh.maps[2].tv[i].y = 1.0f - fh.ReadReal32( );
			}
		}
		if( vertformat & RMVF_UVMAP3 )
		{
			for( int i = 0; i < vertcount; i++ )
			{
				fh.ReadReal32( );
				fh.ReadReal32( );
			}
		}
		if( vertformat & RMVF_UVMAP4 )
		{
			for( int i = 0; i < vertcount; i++ )
			{
				fh.ReadReal32( );
				fh.ReadReal32( );
			}
		}

		int facecount = fh.ReadInt16( );
		msh.setNumFaces( facecount );
		//msh.setNumTVFaces( facecount );
		for( int i = 0; i < facecount; i++ )
		{
			msh.faces[i].v[0] = fh.ReadInt16( );
			msh.faces[i].v[1] = fh.ReadInt16( );
			msh.faces[i].v[2] = fh.ReadInt16( );
			//msh.tvFace[i].setTVerts( msh.faces[i].v );
		}
		///*
		int mc = msh.getNumMaps( );
		for( int i = 1; i < mc; i++ )
		{
			msh.maps[i].setNumFaces( facecount );
			for( int j = 0; j < facecount; j++ )
			{
				msh.maps[i].tf[j].t[0] = msh.faces[j].v[0];
				msh.maps[i].tf[j].t[1] = msh.faces[j].v[1];
				msh.maps[i].tf[j].t[2] = msh.faces[j].v[2];
			}
		}
		//*/

		int stripcount = fh.ReadInt16( );
		fh.Seek( stripcount * 2 );

		fh.ReadInt16( );

		fh.Close( );

		msh.buildBoundingBox( );
		msh.buildNormals( );

		BitArray ba(msh.numVerts);
		ba.SetAll();

		MeshDelta md(msh);
		md.WeldByThreshold(msh,ba,0.1f);
		md.Apply(msh);

		msh.InvalidateGeomCache(); 
		msh.InvalidateTopologyCache(); 
		msh.InvalidateEdgeList( );

		msh.AutoSmooth(60.0f*(3.1415f/180.0f),false);
	};
};

class MapClassDesc
	: public ClassDesc
{
public:
	int IsPublic( ) { return 1; };
	void* Create( BOOL loading = false ) { return new MapImport; }
	const TCHAR* ClassName() { return _T("ROSE Map Importer"); }
	SClass_ID SuperClassID() { return SCENE_IMPORT_CLASS_ID; }
	Class_ID ClassID() { return Class_ID(0x3da419c2,0x26461612); }
	const TCHAR* Category() { return _T("Scene Import"); }
};
static MapClassDesc MapDesc;

bool WINAPI DllMain( HINSTANCE hinstDLL, ULONG fdwReason, LPVOID lpvReserved )
{
	switch( fdwReason )
	{
		case DLL_PROCESS_ATTACH:
		{
			hInstance = hinstDLL;
			DisableThreadLibraryCalls( hInstance );
			break;
		}
	}
	return true;
};

__declspec( dllexport ) const TCHAR* LibDescription( ) { return _T("ROSE Map Importer"); }
__declspec( dllexport ) int LibNumberClasses( ) { return 1; }
__declspec( dllexport ) ClassDesc* LibClassDesc( int i ) { return ( i == 0 ) ? &MapDesc : 0; }
__declspec( dllexport ) ULONG LibVersion( ) { return VERSION_3DSMAX; }
__declspec( dllexport ) ULONG CanAutoDefer() { return 1; }
