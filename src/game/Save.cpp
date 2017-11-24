/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <bzlib.h>
#include <climits>
#include <memory>
#include "Save.h"
#include "BSON.h"
#include "hmap.h" // for firw_data
#include "misc.h" // For restrict_flt
#include "common/Format.h"
#include "simulation/ElementNumbers.h"
#include "simulation/SimulationData.h"
#include "simulation/WallNumbers.h"

Save::Save()
{
	InitData();
	InitVars();
}

void Save::InitData()
{
	blockWidth = blockHeight = 0;
	particlesCount = 0;

	blockMap = NULL;
	fanVelX = NULL;
	fanVelY = NULL;
	particles = NULL;
	pressure = NULL;
	velocityX = NULL;
	velocityY = NULL;
	ambientHeat = NULL;
	//fromNewerVersion = false;
	//hasAmbientHeat = false;
	//authors.clear();
}

void Save::SetSize(int newWidth, int newHeight)
{
	blockWidth = newWidth;
	blockHeight = newHeight;

	particlesCount = 0;
	particles = new particle[NPART];

	blockMap = Allocate2DArray<unsigned char>(blockWidth, blockHeight, 0);
	fanVelX = Allocate2DArray<float>(blockWidth, blockHeight, 0.0f);
	fanVelY = Allocate2DArray<float>(blockWidth, blockHeight, 0.0f);
	pressure = Allocate2DArray<float>(blockWidth, blockHeight, 0.0f);
	velocityX = Allocate2DArray<float>(blockWidth, blockHeight, 0.0f);
	velocityY = Allocate2DArray<float>(blockWidth, blockHeight, 0.0f);
	ambientHeat = Allocate2DArray<float>(blockWidth, blockHeight, 0.0f);
}

void Save::InitVars()
{
	modCreatedVersion = 0;
	androidCreatedVersion = 0;

	waterEEnabled = false;
	legacyEnable = false;
	gravityEnable = false;
	aheatEnable = false;
	paused = false;
	gravityMode = 0;
	airMode = 0;
	edgeMode = 0;
	hasPressure = false;
	hasAmbientHeat = false;
	// jacob1's mod
	hudEnable = true;
	hudEnablePresent = false;
	msRotation = false;
	msRotationPresent = false;
	activeMenu = -1;
	activeMenuPresent = false;
	decorationsEnable = true;
	decorationsEnablePresent = false;
	//translated.x = translated.y = 0;

	saveInfoPresent = false;

	renderModes = std::vector<unsigned int>();
	renderModesPresent = false;
	displayModes = std::vector<unsigned int>();
	displayModesPresent = false;
	colorMode = 0;
	colorModePresent = false;
}

int Save::FixType(int type)
{
	// invalid element, we don't care about it
	if (type < 0 || type > PT_NUM)
		return type;
	else if (modCreatedVersion)
	{
		int max = 161;
		if (modCreatedVersion == 18)
		{
			if (type >= 190 && type <= 204)
				return PT_LOLZ;
		}

		if (createdVersion >= 90)
			max = 179;
		else if (createdVersion >= 89 || modCreatedVersion >= 16)
			max = 177;
		else if (createdVersion >= 87)
			max = 173;
		else if (createdVersion >= 86 || modCreatedVersion == 14)
			max = 170;
		else if (createdVersion >= 84 || modCreatedVersion == 13)
			max = 167;
		else if (modCreatedVersion == 12)
			max = 165;
		else if (createdVersion >= 83)
			max = 163;
		else if (createdVersion >= 82)
			max = 162;

		if (type >= max)
		{
			type += (PT_NORMAL_NUM-max);
		}
		//change VIRS into official elements, and CURE into SOAP; adjust ids
		if (modCreatedVersion <= 15)
		{
			if (type >= PT_NORMAL_NUM+6 && type <= PT_NORMAL_NUM+8)
				type = PT_VIRS + type-(PT_NORMAL_NUM+6);
			else if (type == PT_NORMAL_NUM+9)
				type = PT_SOAP;
			else if (type > PT_NORMAL_NUM+9)
				type -= 4;
		}
		//change GRVT and DRAY into official elements
		if (modCreatedVersion <= 19)
		{
			if (type >= PT_NORMAL_NUM+12 && type <= PT_NORMAL_NUM+13)
				type -= 14;
		}
		//change OTWR and COND into METL, adjust ids
		if (modCreatedVersion <= 20)
		{
			if (type == PT_NORMAL_NUM+3 || type == PT_NORMAL_NUM+9)
				type = PT_METL;
			else if (type > PT_NORMAL_NUM+3 && type < PT_NORMAL_NUM+9)
				type--;
			else if (type > PT_NORMAL_NUM+9)
				type -= 2;
		}
	}
	return type;
}

int Save::ChangeWall(int wt)
{
	if (wt == 1)
		return WL_WALL;
	else if (wt == 2)
		return WL_DESTROYALL;
	else if (wt == 3)
		return WL_ALLOWLIQUID;
	else if (wt == 4)
		return WL_FAN;
	else if (wt == 5)
		return WL_STREAM;
	else if (wt == 6)
		return WL_DETECT;
	else if (wt == 7)
		return WL_EWALL;
	else if (wt == 8)
		return WL_WALLELEC;
	else if (wt == 9)
		return WL_ALLOWAIR;
	else if (wt == 10)
		return WL_ALLOWPOWDER;
	else if (wt == 11)
		return WL_ALLOWALLELEC;
	else if (wt == 12)
		return WL_EHOLE;
	else if (wt == 13)
		return WL_ALLOWGAS;
	return wt;
}

int Save::ChangeWallpp(int wt)
{
	if (wt == O_WL_WALLELEC)
		return WL_WALLELEC;
	else if (wt == O_WL_EWALL)
		return WL_EWALL;
	else if (wt == O_WL_DETECT)
		return WL_DETECT;
	else if (wt == O_WL_STREAM)
		return WL_STREAM;
	else if (wt == O_WL_FAN)
		return WL_FAN;
	else if (wt == O_WL_ALLOWLIQUID)
		return WL_ALLOWLIQUID;
	else if (wt == O_WL_DESTROYALL)
		return WL_DESTROYALL;
	else if (wt == O_WL_WALL)
		return WL_WALL;
	else if (wt == O_WL_ALLOWAIR)
		return WL_ALLOWAIR;
	else if (wt == O_WL_ALLOWSOLID)
		return WL_ALLOWPOWDER;
	else if (wt == O_WL_ALLOWALLELEC)
		return WL_ALLOWALLELEC;
	else if (wt == O_WL_EHOLE)
		return WL_EHOLE;
	else if (wt == O_WL_ALLOWGAS)
		return WL_ALLOWGAS;
	else if (wt == O_WL_GRAV)
		return WL_GRAV;
	else if (wt == O_WL_ALLOWENERGY)
		return WL_ALLOWENERGY;
	return wt;
}

void Save::ParseSave(void *save, int size)
{
	ParseSaveOPS(save, size);
}

void Save::CheckBsonFieldUser(bson_iterator iter, const char *field, unsigned char **data, unsigned int *fieldLen)
{
	if (!strcmp(bson_iterator_key(&iter), field))
	{
		if (bson_iterator_type(&iter) == BSON_BINDATA && ((unsigned char)bson_iterator_bin_type(&iter)) == BSON_BIN_USER && (*fieldLen = bson_iterator_bin_len(&iter)) > 0)
		{
			*data = (unsigned char*)bson_iterator_bin_data(&iter);
		}
		else
		{
			fprintf(stderr, "Invalid datatype for %s: %d[%d] %d[%d] %d[%d]\n", field, bson_iterator_type(&iter), bson_iterator_type(&iter) == BSON_BINDATA, (unsigned char)bson_iterator_bin_type(&iter), ((unsigned char)bson_iterator_bin_type(&iter))==BSON_BIN_USER, bson_iterator_bin_len(&iter), bson_iterator_bin_len(&iter)>0);
		}
	}
}

bool Save::CheckBsonFieldBool(bson_iterator iter, const char *field, bool *flag)
{
	if (!strcmp(bson_iterator_key(&iter), field))
	{
		if (bson_iterator_type(&iter) == BSON_BOOL)
		{
			*flag = bson_iterator_bool(&iter);
			return true;
		}
		else
		{
			fprintf(stderr, "Wrong type for %s, expected bool, got type %i\n", bson_iterator_key(&iter),  bson_iterator_type(&iter));
		}
	}
	return false;
}

bool Save::CheckBsonFieldInt(bson_iterator iter, const char *field, int *setting)
{
	if (!strcmp(bson_iterator_key(&iter), field))
	{
		if (bson_iterator_type(&iter) == BSON_INT)
		{
			*setting = bson_iterator_int(&iter);
			return true;
		}
		else
		{
			fprintf(stderr, "Wrong type for %s, expected int, got type %i\n", bson_iterator_key(&iter),  bson_iterator_type(&iter));
		}
	}
	return false;
}

void Save::ParseSaveOPS(void *save, int size)
{
	unsigned char *inputData = (unsigned char*)save, *bsonData = NULL, *partsData = NULL, *partsPosData = NULL, *fanData = NULL, *wallData = NULL, *soapLinkData = NULL;
	unsigned char *pressData = NULL, *vxData = NULL, *vyData = NULL, *ambientData = NULL;
	unsigned int inputDataLen = size, bsonDataLen = 0, partsDataLen, partsPosDataLen, fanDataLen, wallDataLen, soapLinkDataLen;
	unsigned int pressDataLen, vxDataLen, vyDataLen, ambientDataLen = 0;
#ifndef NOMOD
	unsigned char *movsData = NULL, *animData = NULL;
	unsigned int movsDataLen, animDataLen;
#endif
	unsigned int blockX, blockY, blockW, blockH, fullX, fullY, fullW, fullH;
	createdVersion = inputData[4];

	bson b;
	b.data = NULL;
	bson_iterator iter;
	auto bson_deleter = [](bson * b) { bson_destroy(b); };
	// Use unique_ptr with a custom deleter to ensure that bson_destroy is called even when an exception is thrown
	std::unique_ptr<bson, decltype(bson_deleter)> b_ptr(&b, bson_deleter);

	// Block sizes
	blockX = 0;
	blockY = 0;
	blockW = inputData[6];
	blockH = inputData[7];

	// Full size, normalized
	fullX = blockX*CELL;
	fullY = blockY*CELL;
	fullW = blockW*CELL;
	fullH = blockH*CELL;

	// Incompatible cell size
	if (inputData[5] != CELL)
	{
		throw ParseException("Incorrect CELL size");
	}

	// Too large/off screen
	if (blockX+blockW > XRES/CELL || blockY+blockH > YRES/CELL)
	{
		throw ParseException("Save too large");
	}

	SetSize(blockW, blockH);

	bsonDataLen = ((unsigned)inputData[8]);
	bsonDataLen |= ((unsigned)inputData[9]) << 8;
	bsonDataLen |= ((unsigned)inputData[10]) << 16;
	bsonDataLen |= ((unsigned)inputData[11]) << 24;
	
	// Check for overflows, don't load saves larger than 200MB
	unsigned int toAlloc = bsonDataLen + 1;
	if (toAlloc > 209715200 || !toAlloc)
	{
		throw ParseException("Save data too large");
	}

	bsonData = (unsigned char*)malloc(bsonDataLen+1);
	if (!bsonData)
	{
		throw ParseException("Could not allocate memory");
	}
	// Make sure bsonData is null terminated, since all string functions need null terminated strings
	// (bson_iterator_key returns a pointer into bsonData, which is then used with strcmp)
	bsonData[bsonDataLen] = 0;

	if (BZ2_bzBuffToBuffDecompress((char*)bsonData, (unsigned*)(&bsonDataLen), (char*)inputData+12, inputDataLen-12, 0, 0))
	{
		throw ParseException("Unable to decompress");
	}

	bson_init_data_size(&b, (char*)bsonData, bsonDataLen);
	bson_iterator_init(&iter, &b);
	while (bson_iterator_next(&iter))
	{
		CheckBsonFieldUser(iter, "parts", &partsData, &partsDataLen);
		CheckBsonFieldUser(iter, "partsPos", &partsPosData, &partsPosDataLen);
		CheckBsonFieldUser(iter, "wallMap", &wallData, &wallDataLen);
		CheckBsonFieldUser(iter, "pressMap", &pressData, &pressDataLen);
		CheckBsonFieldUser(iter, "vxMap", &vxData, &vxDataLen);
		CheckBsonFieldUser(iter, "vyMap", &vyData, &vyDataLen);
		CheckBsonFieldUser(iter, "ambientMap", &ambientData, &ambientDataLen);
		CheckBsonFieldUser(iter, "fanMap", &fanData, &fanDataLen);
		CheckBsonFieldUser(iter, "soapLinks", &soapLinkData, &soapLinkDataLen);
#ifndef NOMOD
		CheckBsonFieldUser(iter, "movs", &movsData, &movsDataLen);
		CheckBsonFieldUser(iter, "anim", &animData, &animDataLen);
#endif
		CheckBsonFieldBool(iter, "legacyEnable", &legacyEnable);
		CheckBsonFieldBool(iter, "gravityEnable", &gravityEnable);
		CheckBsonFieldBool(iter, "aheat_enable", &aheatEnable);
		CheckBsonFieldBool(iter, "waterEEnabled", &waterEEnabled);
		CheckBsonFieldBool(iter, "paused", &paused);
		msRotationPresent = CheckBsonFieldBool(iter, "msrotation", &msRotation) || msRotationPresent;
		hudEnablePresent = CheckBsonFieldBool(iter, "hud_enable", &hudEnable) || hudEnablePresent;
		CheckBsonFieldInt(iter, "gravityMode", &gravityMode);
		CheckBsonFieldInt(iter, "airMode", &airMode);
		CheckBsonFieldInt(iter, "edgeMode", &edgeMode);
		activeMenuPresent = CheckBsonFieldInt(iter, "activeMenu", &activeMenu) || activeMenuPresent;
		decorationsEnablePresent = CheckBsonFieldBool(iter, "decorations_enable", &decorationsEnable) || decorationsEnablePresent;

		if (!strcmp(bson_iterator_key(&iter), "signs"))
		{
			if (bson_iterator_type(&iter) == BSON_ARRAY)
			{
				bson_iterator subiter;
				bson_iterator_subiterator(&iter, &subiter);
				while (bson_iterator_next(&subiter))
				{
					if (!strcmp(bson_iterator_key(&subiter), "sign"))
					{
						if (bson_iterator_type(&subiter) == BSON_OBJECT)
						{
							bson_iterator signiter;
							bson_iterator_subiterator(&subiter, &signiter);
							// Stop reading signs if we have no free spaces
							if (signs.size() >= MAXSIGNS)
								break;

							Sign theSign = Sign("", fullX, fullY, Sign::Middle);
							while (bson_iterator_next(&signiter))
							{
								if (!strcmp(bson_iterator_key(&signiter), "text") && bson_iterator_type(&signiter) == BSON_STRING)
								{
									theSign.SetText(Format::CleanString(bson_iterator_string(&signiter), true, true, true).substr(0, 45));
								}
								else if (!strcmp(bson_iterator_key(&signiter), "justification") && bson_iterator_type(&signiter) == BSON_INT)
								{
									int ju = bson_iterator_int(&signiter);
									if (ju >= 0 && ju <= 3)
										theSign.SetJustification((Sign::Justification)bson_iterator_int(&signiter));
								}
								else if (!strcmp(bson_iterator_key(&signiter), "x") && bson_iterator_type(&signiter) == BSON_INT)
								{
									theSign.SetPos(Point(bson_iterator_int(&signiter)+fullX, theSign.GetRealPos().Y));
								}
								else if (!strcmp(bson_iterator_key(&signiter), "y") && bson_iterator_type(&signiter) == BSON_INT)
								{
									theSign.SetPos(Point(theSign.GetRealPos().X, bson_iterator_int(&signiter)+fullY));
								}
								else
								{
									fprintf(stderr, "Unknown sign property %s\n", bson_iterator_key(&signiter));
								}
							}
							signs.push_back(theSign);
						}
						else
						{
							fprintf(stderr, "Wrong type for %s\n", bson_iterator_key(&subiter));
						}
					}
				}
			}
			else
			{
				fprintf(stderr, "Wrong type for %s\n", bson_iterator_key(&iter));
			}
		}
#ifndef NOMOD
#ifdef LUACONSOLE
		else if (!strcmp(bson_iterator_key(&iter), "LuaCode"))
		{
			if (bson_iterator_type(&iter) == BSON_BINDATA && (unsigned char)bson_iterator_bin_type(&iter) == BSON_BIN_USER && bson_iterator_bin_len(&iter) > 0)
			{
				luaCode = bson_iterator_bin_data(&iter);
			}
			else
			{
				fprintf(stderr, "Invalid datatype of anim data: %d[%d] %d[%d] %d[%d]\n", bson_iterator_type(&iter), bson_iterator_type(&iter) == BSON_BINDATA, (unsigned char)bson_iterator_bin_type(&iter), ((unsigned char)bson_iterator_bin_type(&iter)) == BSON_BIN_USER, bson_iterator_bin_len(&iter), bson_iterator_bin_len(&iter)>0);
			}
		}
#endif
#endif
		else if (!strcmp(bson_iterator_key(&iter), "palette"))
		{
			palette.clear();
			if (bson_iterator_type(&iter) == BSON_ARRAY)
			{
				bson_iterator subiter;
				bson_iterator_subiterator(&iter, &subiter);
				while (bson_iterator_next(&subiter))
				{
					if (bson_iterator_type(&subiter) == BSON_INT)
					{
						std::string id = std::string(bson_iterator_key(&subiter));
						int num = bson_iterator_int(&subiter);
						palette.push_back(PaletteItem(id, num));
					}
				}
			}
			else
			{
				fprintf(stderr, "Wrong type for element palette: %d[%d]\n", bson_iterator_type(&iter), bson_iterator_type(&iter)==BSON_ARRAY);
			}
		}
		else if (!strcmp(bson_iterator_key(&iter), "minimumVersion"))
		{
			if (bson_iterator_type(&iter) == BSON_OBJECT)
			{
				int major = INT_MAX, minor = INT_MAX;
				bson_iterator subiter;
				bson_iterator_subiterator(&iter, &subiter);
				while (bson_iterator_next(&subiter))
				{
					if (bson_iterator_type(&subiter) == BSON_INT)
					{
						if (!strcmp(bson_iterator_key(&subiter), "major"))
							major = bson_iterator_int(&subiter);
						else if (!strcmp(bson_iterator_key(&subiter), "minor"))
							minor = bson_iterator_int(&subiter);
						else
							fprintf(stderr, "Wrong type for %s\n", bson_iterator_key(&iter));
					}
				}
				if (major > FAKE_SAVE_VERSION || (major == FAKE_SAVE_VERSION && minor > FAKE_MINOR_VER))
				{
					std::stringstream errorMessage;
					errorMessage << "Save from a newer version: Requires version " << major << "." << minor;
					logMessages.push_back(errorMessage.str());
				}
			}
			else
			{
				fprintf(stderr, "Wrong type for %s\n", bson_iterator_key(&iter));
			}
		}
		else if (!strcmp(bson_iterator_key(&iter), "leftSelectedElementIdentifier") || !strcmp(bson_iterator_key(&iter), "rightSelectedElementIdentifier"))
		{
			if (bson_iterator_type(&iter) == BSON_STRING)
			{
				if (bson_iterator_key(&iter)[0] == 'l')
				{
					leftSelectedIdentifier = bson_iterator_string(&iter);
				}
				else
				{
					rightSelectedIdentifier = bson_iterator_string(&iter);
				}
			}
			else
			{
				fprintf(stderr, "Wrong type for %s\n", bson_iterator_key(&iter));
			}
		}
		else if (!strcmp(bson_iterator_key(&iter), "Jacob1's_Mod"))
		{
			if (bson_iterator_type(&iter)==BSON_INT)
			{
				modCreatedVersion = bson_iterator_int(&iter);
			}
			else
			{
				fprintf(stderr, "Wrong type for %s\n", bson_iterator_key(&iter));
			}
		}
		else if (!strcmp(bson_iterator_key(&iter), "origin"))
		{
			if (bson_iterator_type(&iter) == BSON_OBJECT)
			{
				bson_iterator subiter;
				bson_iterator_subiterator(&iter, &subiter);
				while (bson_iterator_next(&subiter))
				{
					if (!strcmp(bson_iterator_key(&subiter), "mobileBuildVersion"))
					{
						if (bson_iterator_type(&subiter) == BSON_INT)
						{
							androidCreatedVersion =  bson_iterator_int(&subiter);
						}
						else
						{
							fprintf(stderr, "Wrong type for %s\n", bson_iterator_key(&iter));
						}
					}
				}
			}
			else
			{
				fprintf(stderr, "Wrong type for %s\n", bson_iterator_key(&iter));
			}
		}
		else if (!strcmp(bson_iterator_key(&iter), "saveInfo"))
		{
			if (bson_iterator_type(&iter) == BSON_OBJECT)
			{
				bson_iterator saveInfoiter;
				bson_iterator_subiterator(&iter, &saveInfoiter);
				while (bson_iterator_next(&saveInfoiter))
				{
					if (!strcmp(bson_iterator_key(&saveInfoiter), "saveOpened") && bson_iterator_type(&saveInfoiter) == BSON_INT)
						saveInfo.SetSaveOpened(bson_iterator_bool(&saveInfoiter));
					else if (!strcmp(bson_iterator_key(&saveInfoiter), "fileOpened") && bson_iterator_type(&saveInfoiter) == BSON_INT)
						saveInfo.SetFileOpened(bson_iterator_bool(&saveInfoiter));
					else if (!strcmp(bson_iterator_key(&saveInfoiter), "saveName") && bson_iterator_type(&saveInfoiter) == BSON_STRING)
						saveInfo.SetSaveName(bson_iterator_string(&saveInfoiter));
					else if (!strcmp(bson_iterator_key(&saveInfoiter), "fileName") && bson_iterator_type(&saveInfoiter) == BSON_STRING)
						saveInfo.SetFileName(bson_iterator_string(&saveInfoiter));
					else if (!strcmp(bson_iterator_key(&saveInfoiter), "published") && bson_iterator_type(&saveInfoiter) == BSON_INT)
						saveInfo.SetPublished(bson_iterator_bool(&saveInfoiter));
					else if (!strcmp(bson_iterator_key(&saveInfoiter), "ID") && bson_iterator_type(&saveInfoiter) == BSON_STRING)
						saveInfo.SetSaveID(Format::StringToNumber<int>(bson_iterator_string(&saveInfoiter)));
					else if (!strcmp(bson_iterator_key(&saveInfoiter), "description") && bson_iterator_type(&saveInfoiter) == BSON_STRING)
						saveInfo.SetDescription(bson_iterator_string(&saveInfoiter));
					else if (!strcmp(bson_iterator_key(&saveInfoiter), "author") && bson_iterator_type(&saveInfoiter) == BSON_STRING)
						saveInfo.SetAuthor(bson_iterator_string(&saveInfoiter));
					else if (!strcmp(bson_iterator_key(&saveInfoiter), "tags") && bson_iterator_type(&saveInfoiter) == BSON_STRING)
						saveInfo.SetTags(bson_iterator_string(&saveInfoiter));
					else if (!strcmp(bson_iterator_key(&saveInfoiter), "myVote") && bson_iterator_type(&saveInfoiter) == BSON_INT)
						saveInfo.SetMyVote(bson_iterator_int(&saveInfoiter));
					else
						fprintf(stderr, "Unknown save info property %s\n", bson_iterator_key(&saveInfoiter));
				}
				saveInfoPresent = true;
			}
			else
			{
				fprintf(stderr, "Wrong type for %s\n", bson_iterator_key(&iter));
			}
		}
		else if (!strcmp(bson_iterator_key(&iter), "render_modes"))
		{
			bson_iterator subiter;
			bson_iterator_subiterator(&iter, &subiter);
			while (bson_iterator_next(&subiter))
			{
				if (bson_iterator_type(&subiter) == BSON_INT)
				{
					unsigned int renderMode = bson_iterator_int(&subiter);
					renderModes.push_back(renderMode);
				}
			}
			renderModesPresent = true;
		}
		else if (!strcmp(bson_iterator_key(&iter), "display_modes"))
		{
			bson_iterator subiter;
			bson_iterator_subiterator(&iter, &subiter);
			while (bson_iterator_next(&subiter))
			{
				if (bson_iterator_type(&subiter) == BSON_INT)
				{
					unsigned int displayMode = bson_iterator_int(&subiter);
					displayModes.push_back(displayMode);
				}
			}
			displayModesPresent = true;
		}
		else if (!strcmp(bson_iterator_key(&iter), "color_mode") && bson_iterator_type(&iter) == BSON_INT)
		{
			colorMode = bson_iterator_int(&iter);
			colorModePresent = true;
		}
		else if (!strcmp(bson_iterator_key(&iter), "authors"))
		{
			if (bson_iterator_type(&iter) == BSON_OBJECT)
			{
				ConvertBsonToJson(&iter, &authors);
			}
			else
			{
				fprintf(stderr, "Wrong type for %s\n", bson_iterator_key(&iter));
			}
		}
	}


	// Read wall and fan data
	if (wallData)
	{
		unsigned int j = 0;
		if (blockW * blockH > wallDataLen)
			throw ParseException("Not enough wall data");
		for (unsigned int x = 0; x < blockW; x++)
		{
			for (unsigned int y = 0; y < blockH; y++)
			{
				if (wallData[y*blockW+x])
				{
					int wt = ChangeWallpp(wallData[y*blockW+x]);
					if (wt < 0 || wt >= WALLCOUNT)
						continue;
					blockMap[blockY+y][blockX+x] = wt;
				}
				if (wallData[y*blockW+x] == WL_FAN && fanData)
				{
					if (j+1 >= fanDataLen)
					{
						fprintf(stderr, "Not enough fan data\n");
					}
					fanVelX[blockY+y][blockX+x] = (fanData[j++]-127.0f)/64.0f;
					fanVelY[blockY+y][blockX+x] = (fanData[j++]-127.0f)/64.0f;
				}
			}
		}
	}
	
	// Read pressure data
	if (pressData)
	{
		unsigned int j = 0;
		unsigned int i, i2;
		if (blockW * blockH > pressDataLen)
			throw ParseException("Not enough pressure data");
		hasPressure = true;
		for (unsigned int x = 0; x < blockW; x++)
		{
			for (unsigned int y = 0; y < blockH; y++)
			{
				i = pressData[j++];
				i2 = pressData[j++];
				pressure[blockY+y][blockX+x] = ((i+(i2<<8))/128.0f)-256;
			}
		}
	}

	// Read vx data
	if (vxData)
	{
		unsigned int j = 0;
		unsigned int i, i2;
		if (blockW * blockH > vxDataLen)
			throw ParseException("Not enough vx data");
		for (unsigned int x = 0; x < blockW; x++)
		{
			for (unsigned int y = 0; y < blockH; y++)
			{
				i = vxData[j++];
				i2 = vxData[j++];
				velocityX[blockY+y][blockX+x] = ((i+(i2<<8))/128.0f)-256;
			}
		}
	}

	// Read vy data
	if (vyData)
	{
		unsigned int j = 0;
		unsigned int i, i2;
		if (blockW * blockH > vyDataLen)
			throw ParseException("Not enough vy data");
		for (unsigned int x = 0; x < blockW; x++)
		{
			for (unsigned int y = 0; y < blockH; y++)
			{
				i = vyData[j++];
				i2 = vyData[j++];
				velocityY[blockY+y][blockX+x] = ((i+(i2<<8))/128.0f)-256;
			}
		}
	}

	// Read ambient heat data
	if (ambientData)
	{
		unsigned int tempTemp, j = 0;
		if (blockW * blockH > ambientDataLen)
			throw ParseException("Not enough ambient heat data");
		hasAmbientHeat = true;
		for (unsigned int x = 0; x < blockW; x++)
		{
			for (unsigned int y = 0; y < blockH; y++)
			{
				tempTemp = ambientData[j++];
				tempTemp |= (((unsigned)ambientData[j++]) << 8);
				ambientHeat[blockY+y][blockX+x] = tempTemp;
			}
		}
	}

	// Read particle data
	if (partsData && partsPosData)
	{
		int newIndex = 0, fieldDescriptor, tempTemp;
		int posCount, posTotal, partsPosDataIndex = 0;
		if (fullW * fullH * 3 > partsPosDataLen)
			throw ParseException("Not enough particle position data");
		unsigned int i = 0, x, y;
		for (unsigned int saved_y = 0; saved_y < fullH; saved_y++)
		{
			for (unsigned int saved_x = 0; saved_x < fullW; saved_x++)
			{
				// Read total number of particles at this position
				posTotal = 0;
				posTotal |= partsPosData[partsPosDataIndex++]<<16;
				posTotal |= partsPosData[partsPosDataIndex++]<<8;
				posTotal |= partsPosData[partsPosDataIndex++];
				// Put the next posTotal particles at this position
				for (posCount = 0; posCount < posTotal; posCount++)
				{
					// i+3 because we have 4 bytes of required fields (type (1), descriptor (2), temp (1))
					if (i+3 >= partsDataLen)
						throw ParseException("Ran past particle data buffer");
					x = saved_x + fullX;
					y = saved_y + fullY;
					fieldDescriptor = partsData[i+1];
					fieldDescriptor |= partsData[i+2] << 8;
					if (x >= XRES || y >= YRES)
						throw ParseException("Particle out of range");

					if (newIndex < 0 || newIndex >= NPART)
						throw ParseException("Too many particles");

					// Clear the particle, ready for our new properties
					memset(&(particles[newIndex]), 0, sizeof(particle));
					
					// Required fields
					particles[newIndex].type = partsData[i];
					particles[newIndex].x = (float)x;
					particles[newIndex].y = (float)y;
					i+=3;

					// Read temp
					if (fieldDescriptor & 0x01)
					{
						// Full 16bit int
						tempTemp = partsData[i++];
						tempTemp |= (((unsigned)partsData[i++]) << 8);
						particles[newIndex].temp = (float)tempTemp;
					}
					else
					{
						// 1 Byte room temp offset
						tempTemp = (signed char)partsData[i++];
						particles[newIndex].temp = tempTemp+294.15f;
					}
					
					// Read life
					if (fieldDescriptor & 0x02)
					{
						if (i >= partsDataLen)
							throw ParseException("Ran past particle data buffer while loading life");
						particles[newIndex].life = partsData[i++];
						// Read 2nd byte
						if (fieldDescriptor & 0x04)
						{
							if (i >= partsDataLen)
								throw ParseException("Ran past particle data buffer while loading life");
							particles[newIndex].life |= (((unsigned)partsData[i++]) << 8);
						}
					}
					
					// Read tmp
					if (fieldDescriptor & 0x08)
					{
						if (i >= partsDataLen)
							throw ParseException("Ran past particle data buffer while loading tmp");
						particles[newIndex].tmp = partsData[i++];
						// Read 2nd byte
						if (fieldDescriptor & 0x10)
						{
							if (i >= partsDataLen)
								throw ParseException("Ran past particle data buffer while loading tmp");
							particles[newIndex].tmp |= (((unsigned)partsData[i++]) << 8);
							// Read 3rd and 4th bytes
							if (fieldDescriptor & 0x1000)
							{
								if (i+1 >= partsDataLen)
									throw ParseException("Ran past particle data buffer while loading tmp");
								particles[newIndex].tmp |= (((unsigned)partsData[i++]) << 24);
								particles[newIndex].tmp |= (((unsigned)partsData[i++]) << 16);
							}
						}
					}
					
					// Read ctype
					if (fieldDescriptor & 0x20)
					{
						if (i >= partsDataLen)
							throw ParseException("Ran past particle data buffer while loading ctype");
						particles[newIndex].ctype = partsData[i++];
						// Read additional bytes
						if (fieldDescriptor & 0x200)
						{
							if (i+2 >= partsDataLen)
								throw ParseException("Ran past particle data buffer while loading ctype");
							particles[newIndex].ctype |= (((unsigned)partsData[i++]) << 24);
							particles[newIndex].ctype |= (((unsigned)partsData[i++]) << 16);
							particles[newIndex].ctype |= (((unsigned)partsData[i++]) << 8);
						}
					}
					
					// Read dcolor
					if (fieldDescriptor & 0x40)
					{
						if (i+3 >= partsDataLen)
							throw ParseException("Ran past particle data buffer while loading deco");
						unsigned char alpha = partsData[i++];
						unsigned char red = partsData[i++];
						unsigned char green = partsData[i++];
						unsigned char blue = partsData[i++];
						particles[newIndex].dcolour = COLARGB(alpha, red, green, blue);
					}
					
					// Read vx
					if (fieldDescriptor & 0x80)
					{
						if (i >= partsDataLen)
							throw ParseException("Ran past particle data buffer while loading vx");
						particles[newIndex].vx = (partsData[i++]-127.0f)/16.0f;
					}
					
					// Read vy
					if (fieldDescriptor & 0x100)
					{
						if (i >= partsDataLen)
							throw ParseException("Ran past particle data buffer while loading vy");
						particles[newIndex].vy = (partsData[i++]-127.0f)/16.0f;
					}

					// Read tmp2
					if (fieldDescriptor & 0x400)
					{
						if (i >= partsDataLen)
							throw ParseException("Ran past particle data buffer while loading tmp2");
						particles[newIndex].tmp2 = partsData[i++];
						// Read 2nd byte
						if (fieldDescriptor & 0x800)
						{
							if (i >= partsDataLen)
								throw ParseException("Ran past particle data buffer while loading tmp2");
							particles[newIndex].tmp2 |= (((unsigned)partsData[i++]) << 8);
						}
					}

					// Read pavg
					if (fieldDescriptor & 0x2000)
					{
						if (i+3 >= partsDataLen)
							throw ParseException("Ran past particle data buffer while loading pavg");
						int pavg = partsData[i++];
						pavg |= (((unsigned)partsData[i++]) << 8);
						particles[newIndex].pavg[0] = (float)pavg;
						pavg = partsData[i++];
						pavg |= (((unsigned)partsData[i++]) << 8);
						particles[newIndex].pavg[1] = (float)pavg;
					}

					if (modCreatedVersion && modCreatedVersion <= 20)
					{
						// Read flags (for instantly activated powered elements in my mod)
						// now removed so that the partsData save format is exactly the same as tpt and won't cause errors
						if (fieldDescriptor & 0x4000)
						{
							if (i >= partsDataLen)
								throw ParseException("Ran past particle data buffer while loading flags");
							particles[newIndex].flags = partsData[i++];
						}
					}

					// No more particle properties to load, so we can change type here without messing up loading
					if (particles[newIndex].type == PT_SOAP)
						// Delete all soap connections, but it looks like if tmp & tmp2 were saved to 3 bytes, connections would load properly
						particles[newIndex].ctype &= ~6;

					if (createdVersion < 81)
					{
						if (particles[newIndex].type == PT_BOMB && particles[newIndex].tmp != 0)
						{
							particles[newIndex].type = PT_EMBR;
							particles[newIndex].ctype = 0;
							if (particles[newIndex].tmp == 1)
								particles[newIndex].tmp = 0;
						}
						if (particles[newIndex].type == PT_DUST && particles[newIndex].life > 0)
						{
							particles[newIndex].type = PT_EMBR;
							particles[newIndex].ctype = (particles[newIndex].tmp2<<16) | (particles[newIndex].tmp<<8) | particles[newIndex].ctype;
							particles[newIndex].tmp = 1;
						}
						if (particles[newIndex].type == PT_FIRW && particles[newIndex].tmp >= 2)
						{
							int caddress = (int)restrict_flt(restrict_flt((float)(particles[newIndex].tmp-4), 0.0f, 200.0f)*3, 0.0f, (200.0f*3)-3);
							particles[newIndex].type = PT_EMBR;
							particles[newIndex].tmp = 1;
							particles[newIndex].ctype = (((unsigned char)(firw_data[caddress]))<<16) | (((unsigned char)(firw_data[caddress+1]))<<8) | ((unsigned char)(firw_data[caddress+2]));
						}
					}
					if (createdVersion < 87 && particles[newIndex].type == PT_PSTN && particles[newIndex].ctype)
						particles[newIndex].life = 1;
					if (createdVersion < 89)
					{
						if (particles[newIndex].type == PT_FILT)
						{
							if (particles[newIndex].tmp < 0 || particles[newIndex].tmp > 3)
								particles[newIndex].tmp = 6;
							particles[newIndex].ctype = 0;
						}
						else if (particles[newIndex].type == PT_QRTZ || particles[newIndex].type == PT_PQRT)
						{
							particles[newIndex].tmp2 = particles[newIndex].tmp;
							particles[newIndex].tmp = particles[newIndex].ctype;
							particles[newIndex].ctype = 0;
						}
					}
					if (createdVersion < 90)
					{
						if (particles[newIndex].type == PT_PHOT)
							particles[newIndex].flags |= FLAG_PHOTDECO;
					}
					if (createdVersion < 91)
					{
						if (particles[newIndex].type == PT_VINE)
							particles[newIndex].tmp = 1;
						else if (particles[newIndex].type == PT_PSTN)
							particles[newIndex].temp = 283.15;
						else if (particles[newIndex].type == PT_DLAY)
							particles[newIndex].temp = particles[newIndex].temp - 1.0f;
						else if (particles[newIndex].type == PT_CRAY)
						{
							if (particles[newIndex].tmp2)
							{
								particles[newIndex].ctype |= particles[newIndex].tmp2<<8;
								//particles[newIndex].tmp2 = 0;
							}
						}
						else if (particles[newIndex].type == PT_CONV)
						{
							if (particles[newIndex].tmp)
							{
								particles[newIndex].ctype |= particles[newIndex].tmp<<8;
								// particles[newIndex].tmp = 0;
							}
						}
					}
					// Note: PSv was used in version 77.0 and every version before, add something in PSv too if the element is that old

					newIndex++;
					particlesCount++;
				}
			}
		}
#ifndef NOMOD
		if (movsData)
		{
			int movsDataPos = 0;
			for (unsigned int i = 0; i < movsDataLen/2; i++)
			{
				MOVSdataItem data = std::pair<int, int>(movsData[movsDataPos], movsData[movsDataPos+1]);
				MOVSdata.push_back(data);
				movsDataPos += 2;
			}
		}
		if (animData)
		{
			unsigned int animDataPos = 0;
			for (unsigned i = 0; i < particlesCount; i++)
			{
				if (particles[i].type == PT_ANIM)
				{
					if (animDataPos >= animDataLen)
						break;

					ANIMdataItem data;
					int animLen = animData[animDataPos++];
					data.first = animLen;
					if (animDataPos+4*animLen > animDataLen)
						throw ParseException("Ran past particle data buffer while loading animation data");

					for (int j = 0; j <= animLen; j++)
					{
						unsigned char alpha = animData[animDataPos++];
						unsigned char red = animData[animDataPos++];
						unsigned char green = animData[animDataPos++];
						unsigned char blue = animData[animDataPos++];
						data.second.push_back(COLARGB(alpha, red, green, blue));
					}
					ANIMdata.push_back(data);
				}
			}
		}
#endif
		if (soapLinkData)
		{
			unsigned int soapLinkDataPos = 0;
			for (unsigned int i = 0; i < particlesCount; i++)
			{
				if (particles[i].type == PT_SOAP)
				{
					// Get the index of the particle forward linked from this one, if present in the save data
					unsigned int linkedIndex = 0;
					if (soapLinkDataPos+3 > soapLinkDataLen) break;
					linkedIndex |= soapLinkData[soapLinkDataPos++]<<16;
					linkedIndex |= soapLinkData[soapLinkDataPos++]<<8;
					linkedIndex |= soapLinkData[soapLinkDataPos++];
					// All indexes in soapLinkData have 1 added to them (0 means not saved/loaded)
					if (!linkedIndex || linkedIndex-1 >= particlesCount)
						continue;
					linkedIndex = linkedIndex-1;

					// Attach the two particles
					particles[i].ctype |= 2;
					particles[i].tmp = linkedIndex;
					particles[linkedIndex].ctype |= 4;
					particles[linkedIndex].tmp2 = i;
				}
			}
		}
	}

	if (androidCreatedVersion)
		adminLogMessages.push_back("Made in android build version " + Format::NumberToString<int>(androidCreatedVersion));

	if (modCreatedVersion && !androidCreatedVersion)
		adminLogMessages.push_back("Made in jacob1's mod version " + Format::NumberToString<int>(modCreatedVersion));
}

template <typename T>
T ** Save::Allocate2DArray(int blockWidth, int blockHeight, T defaultVal)
{
	T ** temp = new T*[blockHeight];
	for (int y = 0; y < blockHeight; y++)
	{
		temp[y] = new T[blockWidth];
		std::fill(&temp[y][0], &temp[y][0]+blockWidth, defaultVal);
	}
	return temp;
}

// deallocates a pointer to a 2D array and sets it to NULL
template <typename T>
void Save::Deallocate2DArray(T ***array, int blockHeight)
{
	if (*array)
	{
		for (int y = 0; y < blockHeight; y++)
			delete[] (*array)[y];
		delete[] (*array);
		*array = NULL;
	}
}

void Save::ConvertBsonToJson(bson_iterator *iter, Json::Value *j, int depth)
{
	bson_iterator subiter;
	bson_iterator_subiterator(iter, &subiter);
	while (bson_iterator_next(&subiter))
	{
		std::string key = bson_iterator_key(&subiter);
		if (bson_iterator_type(&subiter) == BSON_STRING)
			(*j)[key] = bson_iterator_string(&subiter);
		else if (bson_iterator_type(&subiter) == BSON_BOOL)
			(*j)[key] = bson_iterator_bool(&subiter);
		else if (bson_iterator_type(&subiter) == BSON_INT)
			(*j)[key] = bson_iterator_int(&subiter);
		else if (bson_iterator_type(&subiter) == BSON_LONG)
			(*j)[key] = (Json::Value::UInt64)bson_iterator_long(&subiter);
		else if (bson_iterator_type(&subiter) == BSON_ARRAY && depth < 5)
		{
			bson_iterator arrayiter;
			bson_iterator_subiterator(&subiter, &arrayiter);
			int length = 0, length2 = 0;
			while (bson_iterator_next(&arrayiter))
			{
				if (bson_iterator_type(&arrayiter) == BSON_OBJECT && !strcmp(bson_iterator_key(&arrayiter), "part"))
				{
					Json::Value tempPart;
					ConvertBsonToJson(&arrayiter, &tempPart, depth + 1);
					(*j)["links"].append(tempPart);
					length++;
				}
				else if (bson_iterator_type(&arrayiter) == BSON_INT && !strcmp(bson_iterator_key(&arrayiter), "saveID"))
				{
					(*j)["links"].append(bson_iterator_int(&arrayiter));
				}
				length2++;
				if (length > (int)(40 / ((depth+1) * (depth+1))) || length2 > 50)
					break;
			}
		}
	}
}

std::set<int> Save::GetNestedSaveIDs(Json::Value j)
{
	Json::Value::Members members = j.getMemberNames();
	std::set<int> saveIDs = std::set<int>();
	for (Json::Value::Members::iterator iter = members.begin(), end = members.end(); iter != end; ++iter)
	{
		std::string member = *iter;
		if (member == "id" && j[member].isInt())
			saveIDs.insert(j[member].asInt());
		else if (j[member].isArray())
		{
			for (Json::Value::ArrayIndex i = 0; i < j[member].size(); i++)
			{
				// only supports objects and ints here because that is all we need
				if (j[member][i].isInt())
				{
					saveIDs.insert(j[member][i].asInt());
					continue;
				}
				if (!j[member][i].isObject())
					continue;
				std::set<int> nestedSaveIDs = GetNestedSaveIDs(j[member][i]);
				saveIDs.insert(nestedSaveIDs.begin(), nestedSaveIDs.end());
			}
		}
	}
	return saveIDs;
}

// converts a json object to bson
void Save::ConvertJsonToBson(bson *b, Json::Value j, int depth)
{
	Json::Value::Members members = j.getMemberNames();
	for (Json::Value::Members::iterator iter = members.begin(), end = members.end(); iter != end; ++iter)
	{
		std::string member = *iter;
		if (j[member].isString())
			bson_append_string(b, member.c_str(), j[member].asCString());
		else if (j[member].isBool())
			bson_append_bool(b, member.c_str(), j[member].asBool());
		else if (j[member].type() == Json::intValue)
			bson_append_int(b, member.c_str(), j[member].asInt());
		else if (j[member].type() == Json::uintValue)
			bson_append_long(b, member.c_str(), j[member].asInt64());
		else if (j[member].isArray())
		{
			bson_append_start_array(b, member.c_str());
			std::set<int> saveIDs = std::set<int>();
			int length = 0;
			for (Json::Value::ArrayIndex i = 0; i < j[member].size(); i++)
			{
				// only supports objects and ints here because that is all we need
				if (j[member][i].isInt())
				{
					saveIDs.insert(j[member][i].asInt());
					continue;
				}
				if (!j[member][i].isObject())
					continue;
				if (depth > 4 || length > (int)(40 / ((depth+1) * (depth+1))))
				{
					std::set<int> nestedSaveIDs = GetNestedSaveIDs(j[member][i]);
					saveIDs.insert(nestedSaveIDs.begin(), nestedSaveIDs.end());
				}
				else
				{
					bson_append_start_object(b, "part");
					ConvertJsonToBson(b, j[member][i], depth+1);
					bson_append_finish_object(b);
				}
				length++;
			}
			for (std::set<int>::iterator iter = saveIDs.begin(), end = saveIDs.end(); iter != end; ++iter)
			{
				bson_append_int(b, "saveID", *iter);
			}
			bson_append_finish_array(b);
		}
	}
}
