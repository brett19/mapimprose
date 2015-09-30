/******************************************\
  roseStringTable.hpp

  Part of the Aurora client
  Designed and Coded by Brett LV
\******************************************/

#pragma once

#include "haDatatypes.hpp"
#include "haFile.hpp"

class roseDataTable
{
protected:
	haFile m_File;
	haInt32 m_DataOffset;
	haInt32 m_RecordCount;
	haInt32 m_FieldCount;
	haInt32** m_RowLookup;

public:
	roseDataTable( )
	{
		m_RowLookup = 0;
	};

	~roseDataTable( )
	{
		Cleanup( );
	};

	haBool32 Load( const haString filename )
	{
		if( !m_File.Open( filename, "rb" ) )
			return false;
		
		m_File.Seek( 4 );
		m_DataOffset = m_File.ReadInt32( );
		m_RecordCount = m_File.ReadInt32( ) - 1;
		m_FieldCount = m_File.ReadInt32( ) - 1;
		m_File.Seek( m_DataOffset, SEEK_SET );

		haInt32 strcount = m_RecordCount * m_FieldCount;
		m_RowLookup = new haInt32*[ m_RecordCount ];
		haInt32* tmp = new haInt32[ strcount ];

		for( int i = 0, j = 0; i < strcount; i++ )
		{
			if( i % m_FieldCount == 0 ) {
				m_RowLookup[j] = &tmp[ i ];
				j++;
			}

			tmp[i] = m_File.Tell( );
			m_File.SkipSString( );
		}

		return true;
	};

	void Cleanup( )
	{
		if( m_RowLookup != 0 )
		{
			if( m_RowLookup[0] != 0 )
				delete[] m_RowLookup[0];
			delete[] m_RowLookup;
			m_RowLookup = 0;
		}
	};

	haBool32 ReadString( haInt32 row, haInt32 col, haString text )
	{
		if( row < 0 || row >= m_RecordCount )
			return false;
		if( col < 0 || col >= m_FieldCount )
			return false;

		m_File.Seek( m_RowLookup[row][col], SEEK_SET );
		m_File.ReadSString( text );

		return true;
	};

	haInt32 ReadInt( haInt32 row, haInt32 col )
	{
		haChar tmp[ 32 ];
		if( !ReadString( row, col, tmp ) )
			return 0;
		return atoi( tmp );
	};

	haInt32 ColumnCount( ) const
	{
		return m_FieldCount;
	}

	haInt32 RecordCount( ) const
	{
		return m_RecordCount;
	};

	void Close( )
	{
		m_File.Close( );
	};
};