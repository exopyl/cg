/*****************************************************************************/
//
// File: Defines_3DS.h
//
// Desc: All the chunks defined and structures used to handle the 3DS format
//
/*****************************************************************************/

#ifndef _DEFINES_3DS_HEADER_
#define _DEFINES_3DS_HEADER_

//-----------------------------------------------------------------------------
// 0xxxH Group
//-------
#define	CHK3DS_0_NULL_CHUNK			0x0000 	 
#define	CHK3DS_0_CHUNK_UNKNOWN_0	0x0001 	//Unknown chunk float ??? 
#define	CHK3DS_0_M3D_VERSION		0x0002 	//This gives the version of the 3ds file - short version 
#define	CHK3DS_0_M3D_KFVERSION		0x0005 	 
#define	CHK3DS_0_COLOR_F			0x0010 	//float red, grn, blu; 
#define	CHK3DS_0_COLOR_24			0x0011	//char red, grn, blu; 
#define	CHK3DS_0_LIN_COLOR_24		0x0012	//char red, grn, blu; 
#define	CHK3DS_0_LIN_COLOR_F		0x0013	//float red, grn, blu; 
#define	CHK3DS_0_INT_PERCENTAGE		0x0030	//short percentage; 
#define	CHK3DS_0_FLOAT_PERCENTAGE	0x0031	//float percentage; 
#define	CHK3DS_0_MASTER_SCALE		0x0100	//float scale; 
#define	CHK3DS_0_CHUNKTYPE			0x0995	//ChunkType 
#define	CHK3DS_0_CHUNKUNIQUE		0x0996	//ChunkUnique 
#define	CHK3DS_0_NOTCHUNK			0x0997	//NotChunk 
#define	CHK3DS_0_CONTAINER			0x0998	//Container 
#define	CHK3DS_0_ISCHUNK			0x0999	//IsChunk 
#define	CHK3DS_0_SXP_SELFI_MASKDATA	0x0c3c 	 

//-----------------------------------------------------------------------------
// 1xxx Group
//-------
#define	CHK3DS_1_BIT_MAP			0x1100 		//cstr filename; 
#define	CHK3DS_1_USE_BIT_MAP		0x1101 	 
#define	CHK3DS_1_SOLID_BGND			0x1200 		//followed by color_f 
#define	CHK3DS_1_USE_SOLID_BGND		0x1201 	 
#define	CHK3DS_1_V_GRADIENT			0x1300 		// followed by three color_f: start, mid, endfloat midpoint; 	
#define	CHK3DS_1_USE_V_GRADIENT		0x1301 	 	
#define	CHK3DS_1_LO_SHADOW_BIAS		0x1400 		//float bias; 	
#define	CHK3DS_1_HI_SHADOW_BIAS		0x1410 	 	
#define	CHK3DS_1_SHADOW_MAP_SIZE	0x1420 		//short size; 	
#define	CHK3DS_1_SHADOW_SAMPLES		0x1430 	 	
#define	CHK3DS_1_SHADOW_RANGE		0x1440 	 	
#define	CHK3DS_1_SHADOW_FILTER		0x1450 		//float filter; 	
#define	CHK3DS_1_RAY_BIAS			0x1460 		//float bias; 	
#define	CHK3DS_1_O_CONSTS			0x1500 		//float plane_x, plane_y, plane_z; 	

//-----------------------------------------------------------------------------
// 2xxx Group
//-------
#define	CHK3DS_2_AMBIENT_LIGHT		0x2100 	 
#define	CHK3DS_2_FOG				0x2200 		// followed by color_f, fog_bgndfloat near_plane, near_density;float far_plane, far_density; 
#define	CHK3DS_2_USE_FOG			0x2201 	 
#define	CHK3DS_2_FOG_BGND			0x2210 	 
#define	CHK3DS_2_DISTANCE_CUE		0x2300 		//followed by dcue_bgndfloat near_plane, near_density;float far_plane, far_density; 
#define	CHK3DS_2_USE_DISTANCE_CUE	0x2301 	 
#define	CHK3DS_2_LAYER_FOG			0x2302 		//float fog_z_from, fog_z_to;float fog_density;short fog_type; 
#define	CHK3DS_2_USE_LAYER_FOG		0x2303 	 
#define	CHK3DS_2_DCUE_BGND			0x2310 	 
#define	CHK3DS_2_SMAGIC				0x2d2d 	 
#define	CHK3DS_2_LMAGIC				0x2d3d 	 

//-----------------------------------------------------------------------------
// 3xxx Group
//-------
#define	CHK3DS_3_DEFAULT_VIEW		0x3000 	 
#define	CHK3DS_3_VIEW_TOP			0x3010 		//float targe_x, target_y, target_z;float view_width; 
#define	CHK3DS_3_VIEW_BOTTOM		0x3020 		//float targe_x, target_y, target_z;float view_width; 
#define	CHK3DS_3_VIEW_LEFT			0x3030 		//float targe_x, target_y, target_z;float view_width; 
#define	CHK3DS_3_VIEW_RIGHT			0x3040 		//float targe_x, target_y, target_z;float view_width; 
#define	CHK3DS_3_VIEW_FRONT			0x3050 		//float targe_x, target_y, target_z;float view_width; 
#define	CHK3DS_3_VIEW_BACK			0x3060 		//float targe_x, target_y, target_z;float view_width; 
#define	CHK3DS_3_VIEW_USER			0x3070 		//float targe_x, target_y, target_z;float view_width; 
#define	CHK3DS_3_VIEW_CAMERA		0x3080 		//cstr camera_name; 
#define	CHK3DS_3_VIEW_WINDOW		0x3090 	 
#define	CHK3DS_3_MDATA				0x3d3d 		//Mesh Data Magic Number (.3DS files sub of 4d4d) 
#define	CHK3DS_3_MESH_VERSION		0x3d3e 	 
#define	CHK3DS_3_MLIBMAGIC			0x3daa 		//Material Library Magic Number (.MLI files) 
#define	CHK3DS_3_PRJMAGIC			0x3dc2 		//3dS Project Magic Number (.PRJ files) 
#define	CHK3DS_3_MATMAGIC			0x3dff 		//Material File Magic Number (.MAT files) 

//-----------------------------------------------------------------------------
// 4xxxH Group
//-------
#define	CHK3DS_4_NAMED_OBJECT			0x4000 	//cstr name; 
#define	CHK3DS_4_OBJ_HIDDEN				0x4010 	 
#define	CHK3DS_4_OBJ_VIS_LOFTER			0x4011 	 
#define	CHK3DS_4_OBJ_DOESNT_CAST		0x4012 	 
#define	CHK3DS_4_OBJ_MATTE				0x4013 	 
#define	CHK3DS_4_OBJ_FAST				0x4014 	 
#define	CHK3DS_4_OBJ_PROCEDURAL			0x4015 	 
#define	CHK3DS_4_OBJ_FROZEN				0x4016 	 
#define	CHK3DS_4_OBJ_DONT_RCVSHADOW		0x4017 	 
#define	CHK3DS_4_N_TRI_OBJECT			0x4100 		//named triangle objectfollowed by point_array, point_flag_array, mesh_matrix,face_array 
#define	CHK3DS_4_POINT_ARRAY			0x4110 		//short npoints;struct {float x, y, z;} points[npoints]; 
#define	CHK3DS_4_POINT_FLAG_ARRAY		0x4111 		//short nflags;short flags[nflags]; 
#define	CHK3DS_4_FACE_ARRAY				0x4120 		//may be followed by smooth_groupshort nfaces;struct {short vertex1, vertex2, vertex3;short flags;} facearray[nfaces]; 
#define	CHK3DS_4_MSH_MAT_GROUP			0x4130 		//mesh_material_groupcstr material_name;short nfaces;short facenum[nfaces]; 
#define	CHK3DS_4_OLD_MAT_GROUP			0x4131 	 
#define	CHK3DS_4_TEX_VERTS				0x4140 		//short nverts;struct {float x, y;} vertices[nverts]; 
#define	CHK3DS_4_SMOOTH_GROUP			0x4150 		//short grouplist[n]; determined by length, seems to be 4 per face 
#define	CHK3DS_4_MESH_MATRIX			0x4160 		//float matrix[4][3]; 
#define	CHK3DS_4_MESH_COLOR				0x4165 		//short color_index; 
#define	CHK3DS_4_MESH_TEXTURE_INFO		0x4170 		//short map_type;float x_tiling, y_tiling;float icon_x, icon_y, icon_z;float matrix[4][3];float scaling, plan_icon_w, plan_icon_h, cyl_icon_h; 
#define	CHK3DS_4_PROC_NAME				0x4181 	 
#define	CHK3DS_4_PROC_DATA				0x4182 	 
#define	CHK3DS_4_MSH_BOXMAP				0x4190 	 
#define	CHK3DS_4_N_D_L_OLD				0x4400 	 
#define	CHK3DS_4_N_CAM_OLD				0x4500 	 
#define	CHK3DS_4_N_DIRECT_LIGHT			0x4600 		//followed by color_ffloat x, y, z; 
#define	CHK3DS_4_DL_SPOTLIGHT			0x4610 		//float target_x, target_y, target_z;float hotspot_ang;float falloff_ang; 
#define	CHK3DS_4_DL_OFF					0x4620 	 
#define	CHK3DS_4_DL_ATTENUATE			0x4625 	 
#define	CHK3DS_4_DL_RAYSHAD				0x4627 	 
#define	CHK3DS_4_DL_SHADOWED			0x4630 	 
#define	CHK3DS_4_DL_LOCAL_SHADOW		0x4640 	 
#define	CHK3DS_4_DL_LOCAL_SHADOW2		0x4641 	 
#define	CHK3DS_4_DL_SEE_CONE			0x4650 	 
#define	CHK3DS_4_DL_SPOT_RECTANGULAR	0x4651 	 
#define	CHK3DS_4_DL_SPOT_OVERSHOOT		0x4652 	 
#define	CHK3DS_4_DL_SPOT_PROJECTOR		0x4653 	 
#define	CHK3DS_4_DL_EXCLUDE				0x4654 	 
#define	CHK3DS_4_DL_RANGE				0x4655 	 
#define	CHK3DS_4_DL_SPOT_ROLL			0x4656 		//float roll_ang; 
#define	CHK3DS_4_DL_SPOT_ASPECT			0x4657 	 
#define	CHK3DS_4_DL_RAY_BIAS			0x4658 		//float bias; 
#define	CHK3DS_4_DL_INNER_RANGE			0x4659 		//float range; 
#define	CHK3DS_4_DL_OUTER_RANGE			0x465a 		//float range; 
#define	CHK3DS_4_DL_MULTIPLIER			0x465b 		//float multiple; 
#define	CHK3DS_4_N_AMBIENT_LIGHT		0x4680 	 
#define	CHK3DS_4_N_CAMERA				0x4700 		//float camera_x, camera_y, camera_z;float target_x, target_y, target_z;float bank_angle;float focus; 
#define	CHK3DS_4_CAM_SEE_CONE			0x4710 	 
#define	CHK3DS_4_CAM_RANGES				0x4720 		//float near_range, far_range; 
#define	CHK3DS_4_M3DMAGIC				0x4d4d 		//3DS Magic Number (.3DS file) 
#define	CHK3DS_4_HIERARCHY				0x4f00 	 
#define	CHK3DS_4_PARENT_OBJECT			0x4f10 	 
#define	CHK3DS_4_PIVOT_OBJECT			0x4f20 	 
#define	CHK3DS_4_PIVOT_LIMITS			0x4f30 	 
#define	CHK3DS_4_PIVOT_ORDER			0x4f40 	 
#define	CHK3DS_4_XLATE_RANGE			0x4f50 	 

//-----------------------------------------------------------------------------
// 5xxx Group
//-------
#define	CHK3DS_5_POLY_2D				0x5000 	 
#define	CHK3DS_5_SHAPE_OK				0x5010 	 
#define	CHK3DS_5_SHAPE_NOT_OK			0x5011 	 
#define	CHK3DS_5_SHAPE_HOOK				0x5020 	 

//-----------------------------------------------------------------------------
// 6xxx Group
//-------
#define	CHK3DS_6_PATH_3D				0x6000 	 
#define	CHK3DS_6_PATH_MATRIX			0x6005 	 
#define	CHK3DS_6_SHAPE_2D				0x6010 	 
#define	CHK3DS_6_M_SCALE				0x6020 	 
#define	CHK3DS_6_M_TWIST				0x6030 	 
#define	CHK3DS_6_M_TEETER				0x6040 	 
#define	CHK3DS_6_M_FIT					0x6050 	 
#define	CHK3DS_6_M_BEVEL				0x6060 	 
#define	CHK3DS_6_XZ_CURVE				0x6070 	 
#define	CHK3DS_6_YZ_CURVE				0x6080 	 
#define	CHK3DS_6_INTERPCT				0x6090 	 
#define	CHK3DS_6_DEFORM_LIMIT			0x60a0 	 
#define	CHK3DS_6_USE_CONTOUR			0x6100 	 
#define	CHK3DS_6_USE_TWEEN				0x6110 	 
#define	CHK3DS_6_USE_SCALE				0x6120 	 
#define	CHK3DS_6_USE_TWIST				0x6130 	 
#define	CHK3DS_6_USE_TEETER				0x6140 	 
#define	CHK3DS_6_USE_FIT				0x6150 	 
#define	CHK3DS_6_USE_BEVEL				0x6160 	 

//-----------------------------------------------------------------------------
// 7xxx Group
//-------
#define	CHK3DS_7_VIEWPORT_LAYOUT_OLD	0x7000 	 
#define	CHK3DS_7_VIEWPORT_LAYOUT		0x7001 		//followed by viewport_size, viewport_datashort form, top, ready, wstate, swapws, swapport, swapcur; 
#define	CHK3DS_7_VIEWPORT_DATA_OLD		0x7010 	 
#define	CHK3DS_7_VIEWPORT_DATA			0x7011 		//short flags, axis_lockout;short win_x, win_y, win_w, winh_, win_view;float zoom; float worldcenter_x, worldcenter_y, worldcenter_z;float horiz_ang, vert_ang;cstr camera_name; 
#define	CHK3DS_7_VIEWPORT_DATA_3		0x7012 		//short flags, axis_lockout;short win_x, win_y, win_w, winh_, win_view;float zoom; float worldcenter_x, worldcenter_y, worldcenter_z;float horiz_ang, vert_ang;cstr camera_name; 
#define	CHK3DS_7_VIEWPORT_SIZE			0x7020 		//short x, y, w, h; 
#define	CHK3DS_7_NETWORK_VIEW			0x7030 	 

//-----------------------------------------------------------------------------
// 8xxx Group
//-------
#define	CHK3DS_8_XDATA_SECTION			0x8000 	 
#define	CHK3DS_8_XDATA_ENTRY			0x8001 	 
#define	CHK3DS_8_XDATA_APPNAME			0x8002 	 
#define	CHK3DS_8_XDATA_STRING			0x8003 	 
#define	CHK3DS_8_XDATA_FLOAT			0x8004 	 
#define	CHK3DS_8_XDATA_DOUBLE			0x8005 	 
#define	CHK3DS_8_XDATA_SHORT			0x8006 	 
#define	CHK3DS_8_XDATA_LONG				0x8007 	 
#define	CHK3DS_8_XDATA_VOID				0x8008 	 
#define	CHK3DS_8_XDATA_GROUP			0x8009 	 
#define	CHK3DS_8_XDATA_RFU6				0x800a 	 
#define	CHK3DS_8_XDATA_RFU5				0x800b 	 
#define	CHK3DS_8_XDATA_RFU4				0x800c 	 
#define	CHK3DS_8_XDATA_RFU3				0x800d 	 
#define	CHK3DS_8_XDATA_RFU2				0x800e 	 
#define	CHK3DS_8_XDATA_RFU1				0x800f 	 
#define	CHK3DS_8_PARENT_NAME			0x80f0 	 

//-----------------------------------------------------------------------------
// Axxx Group
//-------
#define	CHK3DS_A_MAT_NAME				0xa000 		//cstr material_name; 
#define	CHK3DS_A_MAT_AMBIENT			0xa010 		//followed by color chunk 
#define	CHK3DS_A_MAT_DIFFUSE			0xa020 		//followed by color chunk 
#define	CHK3DS_A_MAT_SPECULAR			0xa030 		//followed by color chunk 
#define	CHK3DS_A_MAT_SHININESS			0xa040 		//followed by percentage chunk 
#define	CHK3DS_A_MAT_SHIN2PCT			0xa041 		//followed by percentage chunk 
#define	CHK3DS_A_MAT_SHIN3PCT			0xa042 		//followed by percentage chunk 
#define	CHK3DS_A_MAT_TRANSPARENCY		0xa050 		//followed by percentage chunk 
#define	CHK3DS_A_MAT_XPFALL				0xa052 		//followed by percentage chunk 
#define	CHK3DS_A_MAT_REFBLUR			0xa053 		//followed by percentage chunk 
#define	CHK3DS_A_MAT_SELF_ILLUM			0xa080 	
#define	CHK3DS_A_MAT_TWO_SIDE			0xa081 	 
#define	CHK3DS_A_MAT_DECAL				0xa082 	 
#define	CHK3DS_A_MAT_ADDITIVE			0xa083 	 
#define	CHK3DS_A_MAT_SELF_ILPCT			0xa084 		//followed by percentage chunk 
#define	CHK3DS_A_MAT_WIRE				0xa085 	 
#define	CHK3DS_A_MAT_SUPERSMP			0xa086 	 
#define	CHK3DS_A_MAT_WIRESIZE			0xa087 		//float wire_size; 
#define	CHK3DS_A_MAT_FACEMAP			0xa088 	 
#define	CHK3DS_A_MAT_XPFALLIN			0xa08a 	 
#define	CHK3DS_A_MAT_PHONGSOFT			0xa08c 	 
#define	CHK3DS_A_MAT_WIREABS			0xa08e 	 
#define	CHK3DS_A_MAT_SHADING			0xa100 		//short shading_value; 
#define	CHK3DS_A_MAT_TEXMAP				0xa200 		//followed by percentage chunk, mat_mapname,mat_map_tiling, mat_map_texblur... 
#define	CHK3DS_A_MAT_SPECMAP			0xa204 		//followed by percentage_chunk, mat_mapname 
#define	CHK3DS_A_MAT_OPACMAP			0xa210 		//followed by percentage_chunk, mat_mapname 
#define	CHK3DS_A_MAT_REFLMAP			0xa220 		//followed by percentage_chunk, mat_mapname 
#define	CHK3DS_A_MAT_BUMPMAP			0xa230 		//followed by percentage_chunk, mat_mapname 
#define	CHK3DS_A_MAT_USE_XPFALL			0xa240 	 
#define	CHK3DS_A_MAT_USE_REFBLUR		0xa250 	 
#define	CHK3DS_A_MAT_BUMP_PERCENT		0xa252 	 
#define	CHK3DS_A_MAT_MAPNAME			0xa300 		//cstr filename; 
#define	CHK3DS_A_MAT_ACUBIC				0xa310 	 
#define	CHK3DS_A_MAT_SXP_TEXT_DATA		0xa320 	 
#define	CHK3DS_A_MAT_SXP_TEXT2_DATA		0xa321 	 
#define	CHK3DS_A_MAT_SXP_OPAC_DATA		0xa322 	 
#define	CHK3DS_A_MAT_SXP_BUMP_DATA		0xa324 	 
#define	CHK3DS_A_MAT_SXP_SPEC_DATA		0xa325 	 
#define	CHK3DS_A_MAT_SXP_SHIN_DATA		0xa326 	 
#define	CHK3DS_A_MAT_SXP_SELFI_DATA		0xa328 	 
#define	CHK3DS_A_MAT_SXP_TEXT_MASKDATA	0xa32a 	 
#define	CHK3DS_A_MAT_SXP_TEXT2_MASKDATA	0xa32c 	 
#define	CHK3DS_A_MAT_SXP_OPAC_MASKDATA	0xa32e 	 
#define	CHK3DS_A_MAT_SXP_BUMP_MASKDATA	0xa330 	 
#define	CHK3DS_A_MAT_SXP_SPEC_MASKDATA	0xa332 	 
#define	CHK3DS_A_MAT_SXP_SHIN_MASKDATA	0xa334 	 
#define	CHK3DS_A_MAT_SXP_SELFI_MASKDATA	0xa336 	 
#define	CHK3DS_A_MAT_SXP_REFL_MASKDATA	0xa338 	 
#define	CHK3DS_A_MAT_TEX2MAP			0xa33a 	 
#define	CHK3DS_A_MAT_SHINMAP			0xa33c 	 
#define	CHK3DS_A_MAT_SELFIMAP			0xa33d 	 
#define	CHK3DS_A_MAT_TEXMASK			0xa33e 	 
#define	CHK3DS_A_MAT_TEX2MASK			0xa340 	 
#define	CHK3DS_A_MAT_OPACMASK			0xa342 	 
#define	CHK3DS_A_MAT_BUMPMASK			0xa344 	 
#define	CHK3DS_A_MAT_SHINMASK			0xa346 	 
#define	CHK3DS_A_MAT_SPECMASK			0xa348 	 
#define	CHK3DS_A_MAT_SELFIMASK			0xa34a 	 
#define	CHK3DS_A_MAT_REFLMASK			0xa34c 	 
#define	CHK3DS_A_MAT_MAP_TILINGOLD		0xa350 	 
#define	CHK3DS_A_MAT_MAP_TILING			0xa351 		//short flags; 
#define	CHK3DS_A_MAT_MAP_TEXBLUR_OLD	0xa352 	 
#define	CHK3DS_A_MAT_MAP_TEXBLUR		0xa353 		//float blurring; 
#define	CHK3DS_A_MAT_MAP_USCALE			0xa354 	 
#define	CHK3DS_A_MAT_MAP_VSCALE			0xa356 	 
#define	CHK3DS_A_MAT_MAP_UOFFSET		0xa358 	 
#define	CHK3DS_A_MAT_MAP_VOFFSET		0xa35a 	 
#define	CHK3DS_A_MAT_MAP_ANG			0xa35c 	 
#define	CHK3DS_A_MAT_MAP_COL1			0xa360 	 
#define	CHK3DS_A_MAT_MAP_COL2			0xa362 	 
#define	CHK3DS_A_MAT_MAP_RCOL			0xa364 	 
#define	CHK3DS_A_MAT_MAP_GCOL			0xa366 	 
#define	CHK3DS_A_MAT_MAP_BCOL			0xa368 	 
#define	CHK3DS_A_MAT_ENTRY				0xafff //This stored the texture info	 

//-----------------------------------------------------------------------------
// Bxxx Group
//-------
#define	CHK3DS_B_KFDATA				0xb000 		//This is the header for all of the key frame info followed by kfhdr 
#define	CHK3DS_B_AMBIENT_NODE_TAG	0xb001 	 
#define	CHK3DS_B_OBJECT_NODE_TAG	0xb002 		//followed by node_hdr, pivot, pos_track_tag, rot_track_tag, scl_track_tag, morph_smooth... 
#define	CHK3DS_B_CAMERA_NODE_TAG	0xb003 		//followed by node_hdr, pos_track_tag, fov_track_tag, roll_track_tag... 
#define	CHK3DS_B_TARGET_NODE_TAG	0xb004 		//followed by node_hdr, pos_track_tag... 
#define	CHK3DS_B_LIGHT_NODE_TAG		0xb005 		//followed by node_hdr, pos_track_tag, col_track_tag... 
#define	CHK3DS_B_L_TARGET_NODE_TAG	0xb006 		//followed by node_id, node_hdr, pos_track_tag 
#define	CHK3DS_B_SPOTLIGHT_NODE_TAG	0xb007 		//followed by node_id, node_hdr, pos_track_tag, hot_track_tag, fall_track_tag, roll_track_tag, col_track_tag... 
#define	CHK3DS_B_KFSEG				0xb008 		//short start, end; 
#define	CHK3DS_B_KFCURTIME			0xb009 		//short curframe; 
#define	CHK3DS_B_KFHDR				0xb00a 		//followed by viewport_layout, kfseg, kfcurtime, object_node_tag, light_node_tag, target_node_tag, camera_node_tag, l_target_node_tag, spotlight_node_tag, ambient_node_tag...short revision; cstr filename; short animlen; 
#define	CHK3DS_B_NODE_HDR			0xb010 		//cstr objname;short flags1;short flags2; short heirarchy; ? 
#define	CHK3DS_B_INSTANCE_NAME		0xb011 	 
#define	CHK3DS_B_PRESCALE			0xb012 	 
#define	CHK3DS_B_PIVOT				0xb013 		//float pivot_x, pivot_y, pivot_z; 
#define	CHK3DS_B_BOUNDBOX			0xb014 	 
#define	CHK3DS_B_MORPH_SMOOTH		0xb015 		//float morph_smoothing_angle_rad; 
#define	CHK3DS_B_POS_TRACK_TAG		0xb020 		//short flags;short unknown[4];short keys;short unknown;struct {short framenum;long unknown;float pos_x, pos_y, pos_z; } pos[keys]; 
#define	CHK3DS_B_ROT_TRACK_TAG		0xb021 		//short flags;short unknown[4];short keys;short unknown;struct {short framenum;long unknown;float rotation_rad;float axis_x, axis_y, axis_z; } rot[keys]; 
#define	CHK3DS_B_SCL_TRACK_TA		0xb022 		//Gshort flags;short unknown[4];short keys;short unknown;struct {short framenum;long unknown;float scale_x, scale_y, scale_z; } scale[keys]; 
#define	CHK3DS_B_FOV_TRACK_TAG		0xb023 		//short flags;short unknown[4];short keys;short unknown;struct {short framenum;long unknown;float camera_field_of_view;} fov[keys] 
#define	CHK3DS_B_ROLL_TRACK_TAG		0xb024 		//short flags;short unknown[4];short keys;short unknown;struct {short framenum;long unknown;float camera_roll;} roll[keys]; 
#define	CHK3DS_B_COL_TRACK_TAG		0xb025 		//short flags;short unknown[4];short keys;short unknown;struct {short framenum;long unknown;float red, rgn, blu;} color[keys]; 
#define	CHK3DS_B_MORPH_TRACK_TAG	0xb026 		//short flags;short unknown[4];short keys;short unknown;struct {short framenum;long unknown;cstr obj_name;} morph[keys]; 
#define	CHK3DS_B_HOT_TRACK_TAG		0xb027 		//short flags;short unknown[4];short keys;short unknown;struct {short framenum;long unknown;float hotspot_ang;} hotspot[keys]; 
#define	CHK3DS_B_FALL_TRACK_TAG		0xb028 		//short flags;short unknown[4];short keys;short unknown;struct {short framenum;long unknown;float falloff_ang;} falloff[keys]; 
#define	CHK3DS_B_HIDE_TRACK_TAG		0xb029 	 
#define	CHK3DS_B_NODE_ID			0xb030 		//short id; 

//-----------------------------------------------------------------------------
// Cxxx Group
//-------
#define CHK3DS_C_MDRAWER				0xc010
#define CHK3DS_C_TDRAWER				0xc020
#define CHK3DS_C_SHPDRAWER				0xc030
#define CHK3DS_C_MODDRAWER				0xc040
#define CHK3DS_C_RIPDRAWER				0xc050
#define CHK3DS_C_TXDRAWER				0xc060
#define CHK3DS_C_PDRAWER				0xc062
#define CHK3DS_C_MTLDRAWER				0xc064
#define CHK3DS_C_FLIDRAWER				0xc066
#define CHK3DS_C_CUBDRAWER				0xc067
#define CHK3DS_C_MFILE					0xc070
#define CHK3DS_C_SHPFILE				0xc080
#define CHK3DS_C_MODFILE				0xc090
#define CHK3DS_C_RIPFILE				0xc0a0
#define CHK3DS_C_TXFILE					0xc0b0
#define CHK3DS_C_PFILE					0xc0b2
#define CHK3DS_C_MTLFILE				0xc0b4
#define CHK3DS_C_FLIFILE				0xc0b6
#define CHK3DS_C_PALFILE				0xc0b8
#define CHK3DS_C_TX_STRING				0xc0c0
#define CHK3DS_C_CONSTS					0xc0d0
#define CHK3DS_C_SNAPS					0xc0e0
#define CHK3DS_C_GRIDS					0xc0f0
#define CHK3DS_C_ASNAPS					0xc100
#define CHK3DS_C_GRID_RANGE				0xc110
#define CHK3DS_C_RENDTYPE				0xc120
#define CHK3DS_C_PROGMODE				0xc130
#define CHK3DS_C_PREVMODE				0xc140
#define CHK3DS_C_MODWMODE				0xc150
#define CHK3DS_C_MODMODEL				0xc160
#define CHK3DS_C_ALL_LINES				0xc170
#define CHK3DS_C_BACK_TYPE				0xc180
#define CHK3DS_C_MD_CS					0xc190
#define CHK3DS_C_MD_CE					0xc1a0
#define CHK3DS_C_MD_SML					0xc1b0
#define CHK3DS_C_MD_SMW					0xc1c0
#define CHK3DS_C_LOFT_WITH_TEXTURE		0xc1c3
#define CHK3DS_C_LOFT_L_REPEAT			0xc1c4
#define CHK3DS_C_LOFT_W_REPEAT			0xc1c5
#define CHK3DS_C_LOFT_UV_NORMALIZE		0xc1c6
#define CHK3DS_C_WELD_LOFT				0xc1c7
#define CHK3DS_C_MD_PDET				0xc1d0
#define CHK3DS_C_MD_SDET				0xc1e0
#define CHK3DS_C_RGB_RMODE				0xc1f0
#define CHK3DS_C_RGB_HIDE				0xc200
#define CHK3DS_C_RGB_MAPSW				0xc202
#define CHK3DS_C_RGB_TWOSIDE			0xc204
#define CHK3DS_C_RGB_SHADOW				0xc208
#define CHK3DS_C_RGB_AA					0xc210
#define CHK3DS_C_RGB_OVW				0xc220
#define CHK3DS_C_RGB_OVH				0xc230
#define CHK3DS_C_CMAGIC					0xc23d
#define CHK3DS_C_RGB_PICTYPE			0xc240
#define CHK3DS_C_RGB_OUTPUT				0xc250
#define CHK3DS_C_RGB_TODISK				0xc253
#define CHK3DS_C_RGB_COMPRESS			0xc254
#define CHK3DS_C_JPEG_COMPRESSION		0xc255
#define CHK3DS_C_RGB_DISPDEV			0xc256
#define CHK3DS_C_RGB_HARDDEV			0xc259
#define CHK3DS_C_RGB_PATH				0xc25a
#define CHK3DS_C_BITMAP_DRAWER			0xc25b
#define CHK3DS_C_RGB_FILE				0xc260
#define CHK3DS_C_RGB_OVASPECT			0xc270
#define CHK3DS_C_RGB_ANIMTYPE			0xc271
#define CHK3DS_C_RENDER_ALL				0xc272
#define CHK3DS_C_REND_FROM				0xc273
#define CHK3DS_C_REND_TO				0xc274
#define CHK3DS_C_REND_NTH				0xc275
#define CHK3DS_C_PAL_TYPE				0xc276
#define CHK3DS_C_RND_TURBO				0xc277
#define CHK3DS_C_RND_MIP				0xc278
#define CHK3DS_C_BGND_METHOD			0xc279
#define CHK3DS_C_AUTO_REFLECT			0xc27a
#define CHK3DS_C_VP_FROM				0xc27b
#define CHK3DS_C_VP_TO					0xc27c
#define CHK3DS_C_VP_NTH					0xc27d
#define CHK3DS_C_REND_TSTEP				0xc27e
#define CHK3DS_C_VP_TSTEP				0xc27f
#define CHK3DS_C_SRDIAM					0xc280
#define CHK3DS_C_SRDEG					0xc290
#define CHK3DS_C_SRSEG					0xc2a0
#define CHK3DS_C_SRDIR					0xc2b0
#define CHK3DS_C_HETOP					0xc2c0
#define CHK3DS_C_HEBOT					0xc2d0
#define CHK3DS_C_HEHT					0xc2e0
#define CHK3DS_C_HETURNS				0xc2f0
#define CHK3DS_C_HEDEG					0xc300
#define CHK3DS_C_HESEG					0xc310
#define CHK3DS_C_HEDIR					0xc320
#define CHK3DS_C_QUIKSTUFF				0xc330
#define CHK3DS_C_SEE_LIGHTS				0xc340
#define CHK3DS_C_SEE_CAMERAS			0xc350
#define CHK3DS_C_SEE_3D					0xc360
#define CHK3DS_C_MESHSEL				0xc370
#define CHK3DS_C_MESHUNSEL				0xc380
#define CHK3DS_C_POLYSEL				0xc390
#define CHK3DS_C_POLYUNSEL				0xc3a0
#define CHK3DS_C_SHPLOCAL				0xc3a2
#define CHK3DS_C_MSHLOCAL				0xc3a4
#define CHK3DS_C_NUM_FORMAT				0xc3b0
#define CHK3DS_C_ARCH_DENOM				0xc3c0
#define CHK3DS_C_IN_DEVICE				0xc3d0
#define CHK3DS_C_MSCALE					0xc3e0
#define CHK3DS_C_COMM_PORT				0xc3f0
#define CHK3DS_C_TAB_BASES				0xc400
#define CHK3DS_C_TAB_DIVS				0xc410
#define CHK3DS_C_MASTER_SCALES			0xc420
#define CHK3DS_C_SHOW_1STVERT			0xc430
#define CHK3DS_C_SHAPER_OK				0xc440
#define CHK3DS_C_LOFTER_OK				0xc450
#define CHK3DS_C_EDITOR_OK				0xc460
#define CHK3DS_C_KEYFRAMER_OK			0xc470
#define CHK3DS_C_PICKSIZE				0xc480
#define CHK3DS_C_MAPTYPE				0xc490
#define CHK3DS_C_MAP_DISPLAY			0xc4a0
#define CHK3DS_C_TILE_XY				0xc4b0
#define CHK3DS_C_MAP_XYZ				0xc4c0
#define CHK3DS_C_MAP_SCALE				0xc4d0
#define CHK3DS_C_MAP_MATRIX_OLD			0xc4e0
#define CHK3DS_C_MAP_MATRIX				0xc4e1
#define CHK3DS_C_MAP_WID_HT				0xc4f0
#define CHK3DS_C_OBNAME					0xc500
#define CHK3DS_C_CAMNAME				0xc510
#define CHK3DS_C_LTNAME					0xc520
#define CHK3DS_C_CUR_MNAME				0xc525
#define CHK3DS_C_CURMTL_FROM_MESH		0xc526
#define CHK3DS_C_GET_SHAPE_MAKE_FACES	0xc527
#define CHK3DS_C_DETAIL					0xc530
#define CHK3DS_C_VERTMARK				0xc540
#define CHK3DS_C_MSHAX					0xc550
#define CHK3DS_C_MSHCP					0xc560
#define CHK3DS_C_USERAX					0xc570
#define CHK3DS_C_SHOOK					0xc580
#define CHK3DS_C_RAX					0xc590
#define CHK3DS_C_STAPE					0xc5a0
#define CHK3DS_C_LTAPE					0xc5b0
#define CHK3DS_C_ETAPE					0xc5c0
#define CHK3DS_C_KTAPE					0xc5c8
#define CHK3DS_C_SPHSEGS				0xc5d0
#define CHK3DS_C_GEOSMOOTH				0xc5e0
#define CHK3DS_C_HEMISEGS				0xc5f0
#define CHK3DS_C_PRISMSEGS				0xc600
#define CHK3DS_C_PRISMSIDES				0xc610
#define CHK3DS_C_TUBESEGS				0xc620
#define CHK3DS_C_TUBESIDES				0xc630
#define CHK3DS_C_TORSEGS				0xc640
#define CHK3DS_C_TORSIDES				0xc650
#define CHK3DS_C_CONESIDES				0xc660
#define CHK3DS_C_CONESEGS				0xc661
#define CHK3DS_C_NGPARMS				0xc670
#define CHK3DS_C_PTHLEVEL				0xc680
#define CHK3DS_C_MSCSYM					0xc690
#define CHK3DS_C_MFTSYM					0xc6a0
#define CHK3DS_C_MTTSYM					0xc6b0
#define CHK3DS_C_SMOOTHING				0xc6c0
#define CHK3DS_C_MODICOUNT				0xc6d0
#define CHK3DS_C_FONTSEL				0xc6e0
#define CHK3DS_C_TESS_TYPE				0xc6f0
#define CHK3DS_C_TESS_TENSION			0xc6f1
#define CHK3DS_C_SEG_START				0xc700
#define CHK3DS_C_SEG_END				0xc705
#define CHK3DS_C_CURTIME				0xc710
#define CHK3DS_C_ANIMLENGTH				0xc715
#define CHK3DS_C_PV_FROM				0xc720
#define CHK3DS_C_PV_TO					0xc725
#define CHK3DS_C_PV_DOFNUM				0xc730
#define CHK3DS_C_PV_RNG					0xc735
#define CHK3DS_C_PV_NTH					0xc740
#define CHK3DS_C_PV_TYPE				0xc745
#define CHK3DS_C_PV_METHOD				0xc750
#define CHK3DS_C_PV_FPS					0xc755
#define CHK3DS_C_VTR_FRAMES				0xc765
#define CHK3DS_C_VTR_HDTL				0xc770
#define CHK3DS_C_VTR_HD					0xc771
#define CHK3DS_C_VTR_TL					0xc772
#define CHK3DS_C_VTR_IN					0xc775
#define CHK3DS_C_VTR_PK					0xc780
#define CHK3DS_C_VTR_SH					0xc785
#define CHK3DS_C_WORK_MTLS				0xc790
#define CHK3DS_C_WORK_MTLS_2			0xc792
#define CHK3DS_C_WORK_MTLS_3			0xc793
#define CHK3DS_C_WORK_MTLS_4			0xc794
#define CHK3DS_C_BGTYPE					0xc7a1
#define CHK3DS_C_MEDTILE				0xc7b0
#define CHK3DS_C_LO_CONTRAST			0xc7d0
#define CHK3DS_C_HI_CONTRAST			0xc7d1
#define CHK3DS_C_FROZ_DISPLAY			0xc7e0
#define CHK3DS_C_BOOLWELD				0xc7f0
#define CHK3DS_C_BOOLTYPE				0xc7f1
#define CHK3DS_C_ANG_THRESH				0xc900
#define CHK3DS_C_SS_THRESH				0xc901
#define CHK3DS_C_TEXTURE_BLUR_DEFAULT	0xc903
#define CHK3DS_C_MAPDRAWER				0xca00
#define CHK3DS_C_MAPDRAWER1				0xca01
#define CHK3DS_C_MAPDRAWER2				0xca02
#define CHK3DS_C_MAPDRAWER3				0xca03
#define CHK3DS_C_MAPDRAWER4				0xca04
#define CHK3DS_C_MAPDRAWER5				0xca05
#define CHK3DS_C_MAPDRAWER6				0xca06
#define CHK3DS_C_MAPDRAWER7				0xca07
#define CHK3DS_C_MAPDRAWER8				0xca08
#define CHK3DS_C_MAPDRAWER9				0xca09
#define CHK3DS_C_MAPDRAWER_ENTRY		0xca10
#define CHK3DS_C_BACKUP_FILE			0xca20
#define CHK3DS_C_DITHER_256				0xca21
#define CHK3DS_C_SAVE_LAST				0xca22
#define CHK3DS_C_USE_ALPHA				0xca23
#define CHK3DS_C_TGA_DEPTH				0xca24
#define CHK3DS_C_REND_FIELDS			0xca25
#define CHK3DS_C_REFLIP					0xca26
#define CHK3DS_C_SEL_ITEMTOG			0xca27
#define CHK3DS_C_SEL_RESET				0xca28
#define CHK3DS_C_STICKY_KEYINF			0xca29
#define CHK3DS_C_WELD_THRESHOLD			0xca2a
#define CHK3DS_C_ZCLIP_POINT			0xca2b
#define CHK3DS_C_ALPHA_SPLIT			0xca2c
#define CHK3DS_C_KF_SHOW_BACKFACE		0xca30
#define CHK3DS_C_OPTIMIZE_LOFT			0xca40
#define CHK3DS_C_TENS_DEFAULT			0xca42
#define CHK3DS_C_CONT_DEFAULT			0xca44
#define CHK3DS_C_BIAS_DEFAULT			0xca46
#define CHK3DS_C_DXFNAME_SRC			0xca50
#define CHK3DS_C_AUTO_WELD				0xca60
#define CHK3DS_C_AUTO_UNIFY				0xca70
#define CHK3DS_C_AUTO_SMOOTH			0xca80
#define CHK3DS_C_DXF_SMOOTH_ANG			0xca90
#define CHK3DS_C_SMOOTH_ANG				0xcaa0
#define CHK3DS_C_WORK_MTLS_5			0xcb00
#define CHK3DS_C_WORK_MTLS_6			0xcb01
#define CHK3DS_C_WORK_MTLS_7			0xcb02
#define CHK3DS_C_WORK_MTLS_8			0xcb03
#define CHK3DS_C_WORKMTL				0xcb04
#define CHK3DS_C_SXP_TEXT_DATA			0xcb10
#define CHK3DS_C_SXP_OPAC_DATA			0xcb11
#define CHK3DS_C_SXP_BUMP_DATA			0xcb12
#define CHK3DS_C_SXP_SHIN_DATA			0xcb13
#define CHK3DS_C_SXP_TEXT2_DATA			0xcb20
#define CHK3DS_C_SXP_SPEC_DATA			0xcb24
#define CHK3DS_C_SXP_SELFI_DATA			0xcb28
#define CHK3DS_C_SXP_TEXT_MASKDATA		0xcb30
#define CHK3DS_C_SXP_TEXT2_MASKDATA		0xcb32
#define CHK3DS_C_SXP_OPAC_MASKDATA		0xcb34
#define CHK3DS_C_SXP_BUMP_MASKDATA		0xcb36
#define CHK3DS_C_SXP_SPEC_MASKDATA		0xcb38
#define CHK3DS_C_SXP_SHIN_MASKDATA		0xcb3a
#define CHK3DS_C_SXP_REFL_MASKDATA		0xcb3e
#define CHK3DS_C_NET_USE_VPOST			0xcc00
#define CHK3DS_C_NET_USE_GAMMA			0xcc10
#define CHK3DS_C_NET_FIELD_ORDER		0xcc20
#define CHK3DS_C_BLUR_FRAMES			0xcd00
#define CHK3DS_C_BLUR_SAMPLES			0xcd10
#define CHK3DS_C_BLUR_DUR				0xcd20
#define CHK3DS_C_HOT_METHOD				0xcd30
#define CHK3DS_C_HOT_CHECK				0xcd40
#define CHK3DS_C_PIXEL_SIZE				0xcd50
#define CHK3DS_C_DISP_GAMMA				0xcd60
#define CHK3DS_C_FBUF_GAMMA				0xcd70
#define CHK3DS_C_FILE_OUT_GAMMA			0xcd80
#define CHK3DS_C_FILE_IN_GAMMA			0xcd82
#define CHK3DS_C_GAMMA_CORRECT			0xcd84
#define CHK3DS_C_APPLY_DISP_GAMMA		0xcd90
#define CHK3DS_C_APPLY_FBUF_GAMMA		0xcda0
#define CHK3DS_C_APPLY_FILE_GAMMA		0xcdb0
#define CHK3DS_C_FORCE_WIRE				0xcdc0
#define CHK3DS_C_RAY_SHADOWS			0xcdd0
#define CHK3DS_C_MASTER_AMBIENT			0xcde0
#define CHK3DS_C_SUPER_SAMPLE			0xcdf0
#define CHK3DS_C_OBJECT_MBLUR			0xce00
#define CHK3DS_C_MBLUR_DITHER			0xce10
#define CHK3DS_C_DITHER_24				0xce20
#define CHK3DS_C_SUPER_BLACK			0xce30
#define CHK3DS_C_SAFE_FRAME				0xce40
#define CHK3DS_C_VIEW_PRES_RATIO		0xce50
#define CHK3DS_C_BGND_PRES_RATIO		0xce60
#define CHK3DS_C_NTH_SERIAL_NUM			0xce70

//-----------------------------------------------------------------------------
// Dxxx Group
//-------
#define	CHK3DS_D_VPDATA					0xd000 	 
#define	CHK3DS_D_P_QUEUE_ENTRY			0xd100 	 
#define	CHK3DS_D_P_QUEUE_IMAGE			0xd110 	 
#define	CHK3DS_D_P_QUEUE_USEIGAMMA		0xd114 	 
#define	CHK3DS_D_P_QUEUE_PROC			0xd120 	
#define	CHK3DS_D_P_QUEUE_SOLID			0xd130 	
#define	CHK3DS_D_P_QUEUE_GRADIENT		0xd140 	 
#define	CHK3DS_D_P_QUEUE_KF				0xd150 	
#define	CHK3DS_D_P_QUEUE_MOTBLUR		0xd152 	 
#define	CHK3DS_D_P_QUEUE_MB_REPEAT		0xd153 	 
#define	CHK3DS_D_P_QUEUE_NONE			0xd160 	 
#define	CHK3DS_D_P_QUEUE_RESIZE			0xd180 	 
#define	CHK3DS_D_P_QUEUE_OFFSET			0xd185 	 
#define	CHK3DS_D_P_QUEUE_ALIGN			0xd190 	 
#define	CHK3DS_D_P_CUSTOM_SIZE			0xd1a0 	 
#define	CHK3DS_D_P_ALPH_NONE			0xd210 	 
#define	CHK3DS_D_P_ALPH_PSEUDO			0xd220 	 
#define	CHK3DS_D_P_ALPH_OP_PSEUDO		0xd221 	 
#define	CHK3DS_D_P_ALPH_BLUR			0xd222 	 
#define	CHK3DS_D_P_ALPH_PCOL			0xd225 	 
#define	CHK3DS_D_P_ALPH_C0				0xd230 	 
#define	CHK3DS_D_P_ALPH_OP_KEY			0xd231 	 
#define	CHK3DS_D_P_ALPH_KCOL			0xd235 	 
#define	CHK3DS_D_P_ALPH_OP_NOCONV		0xd238 	 
#define	CHK3DS_D_P_ALPH_IMAGE			0xd240 	 
#define	CHK3DS_D_P_ALPH_ALPHA			0xd250 	 
#define	CHK3DS_D_P_ALPH_QUES			0xd260 	 
#define	CHK3DS_D_P_ALPH_QUEIMG			0xd265 	 
#define	CHK3DS_D_P_ALPH_CUTOFF			0xd270 	 
#define	CHK3DS_D_P_ALPHANEG				0xd280 	 
#define	CHK3DS_D_P_TRAN_NONE			0xd300 	 
#define	CHK3DS_D_P_TRAN_IMAGE			0xd310 	 
#define	CHK3DS_D_P_TRAN_FRAMES			0xd312 	 
#define	CHK3DS_D_P_TRAN_FADEIN			0xd320 	 
#define	CHK3DS_D_P_TRAN_FADEOUT			0xd330 	 
#define	CHK3DS_D_P_TRANNEG				0xd340 	 
#define	CHK3DS_D_P_RANGES				0xd400 	 
#define	CHK3DS_D_P_PROC_DATA			0xd500 	 

//-----------------------------------------------------------------------------
// Fxxx Group
//-------
#define	CHK3DS_F_POS_TRACK_TAG_KEY		0xf020 	 
#define	CHK3DS_F_ROT_TRACK_TAG_KEY		0xf021 	 
#define	CHK3DS_F_SCL_TRACK_TAG_KEY		0xf022 	 
#define	CHK3DS_F_FOV_TRACK_TAG_KEY		0xf023 	 
#define	CHK3DS_F_ROLL_TRACK_TAG_KEY		0xf024 	 
#define	CHK3DS_F_COL_TRACK_TAG_KEY		0xf025 	 
#define	CHK3DS_F_MORPH_TRACK_TAG_KEY	0xf026 	 
#define	CHK3DS_F_HOT_TRACK_TAG_KEY		0xf027 	 
#define	CHK3DS_F_FALL_TRACK_TAG_KEY		0xf028 	 
#define	CHK3DS_F_POINT_ARRAY_ENTRY		0xf110 	 
#define	CHK3DS_F_POINT_FLAG_ARRAY_ENTRY	0xf111 	 
#define	CHK3DS_F_FACE_ARRAY_ENTRY		0xf120 	 
#define	CHK3DS_F_MSH_MAT_GROUP_ENTRY	0xf130 	 
#define	CHK3DS_F_TEX_VERTS_ENTRY		0xf140 	 
#define	CHK3DS_F_SMOOTH_GROUP_ENTRY		0xf150 	 
#define	CHK3DS_F_DUMMY					0xffff 	 

#endif	//_DEFINES_3DS_HEADER_
