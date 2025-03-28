/********************************************************************************
 
  Copyright (C) 2017-2018 Intel Corporation
  Lantiq Beteiligungs-GmbH & Co. KG
  Lilienthalstrasse 15, 85579 Neubiberg, Germany 

  For licensing information, see the file 'LICENSE' in the root folder of
  this software module.
 
********************************************************************************/

/* ****************************************************************************** 
 *         File Name    : ugw_list.c                                       	*
 *         Description  : helper Library , which contains list manipulation 	*
 *			  fuctions used across the system 			*
 * ******************************************************************************/
#define _GNU_SOURCE

#include <stdio.h>
#include <string.h>
#include "libsafec/safe_str_lib.h"
#include "libsafec/safe_lib.h"
#include "libsafec/safe_mem_lib.h"
#include "help_objlist.h"

#ifndef LOG_LEVEL
uint16_t LOGLEVEL = SYS_LOG_DEBUG + 1;
#else
uint16_t LOGLEVEL = LOG_LEVEL + 1;
#endif

#ifndef LOG_TYPE
uint16_t LOGTYPE = SYS_LOG_TYPE_FILE;
#else
uint16_t LOGTYPE = LOG_TYPE;
#endif

/*	\brief  Helper function that removes the  index suffix from the string and returns it as integer
	\param[in,out] s - string with index suffix  and "_" separator between them.
	\return for legal value returned the index number, for ERROR cases returned -1
*/
static int indexFromString(char *s);


/*  =============================================================================
 *   Function Name 	: help_storeLocalDB					*
 *   Description 	: Function to dump to objlist to local tmp file.	*
 *  ============================================================================*/
int help_storeLocalDB(ObjList *wlObj, const char *pcPath)
{
	char *paramNamePtr;
	char *paramValuePtr;
	char *objName;
	int objIndex = 0; //running index
	int nRet = UGW_SUCCESS, nFSt=-1;

	ObjList *obj;
	ParamList *param;
	
	FILE *fp=NULL;
	if((nFSt=fopen_s(&fp, pcPath, "w")) != EOK){
		LOGF_LOG_ERROR("Failed to open file for writing : %s\n",pcPath);
		nRet = UGW_FAILURE;
		goto finish;
	}

	if(fp == NULL){
		LOGF_LOG_ERROR("Failed to open file for writing : %s\n",pcPath);
		nRet = UGW_FAILURE;
		goto finish;
	}

	if (!wlObj) {
		LOGF_LOG_ERROR("ERROR Null Pointer WlObj, can't return data \n");
		nRet = UGW_FAILURE;
		goto finish;
	}

	FOR_EACH_OBJ(wlObj, obj) {
		objName = GET_OBJ_NAME(obj);
		//Write Obj name to loacl file
		fprintf(fp, "Object_%d=%s\n", objIndex, objName);

		FOR_EACH_PARAM(obj,param) {
			paramNamePtr = GET_PARAM_NAME(param);
			paramValuePtr = GET_PARAM_VALUE(param);

			fprintf(fp, "%s_%d=%s\n",paramNamePtr, objIndex, paramValuePtr);
		}
		objIndex++;;
	}

finish:
	if(nFSt == EOK && fp != NULL)
		fclose(fp);
	return nRet;
}

/*  =============================================================================
 *   Function Name 	: help_loadLocalDB					*
 *   Description 	: Function to fill to objlist from local tmp file data	*
 *  ============================================================================*/
int help_loadLocalDB(ObjList* wlObj, const char *pcPath)
{
	FILE* fp=NULL;
	char* line = NULL;

	size_t len = 0;
	size_t len1=0;
	
	char* parameterName = NULL;
	char* parameterValue = NULL,*pcSavePtr=NULL;

	int	objCurrentIdx = -1,nFSt=-1;
	char objCurrentName[MAX_LEN_PARAM_NAME];

	ObjList* obj = NULL;
	int nRet = UGW_SUCCESS;

	if((nFSt=fopen_s(&fp, pcPath, "r")) != EOK){
		LOGF_LOG_ERROR("File Not Found : %s\n",pcPath);
		nRet = UGW_FAILURE;
		goto finish;
	}

	if(fp == NULL){
		LOGF_LOG_ERROR("Failed to open file for writing : %s\n",pcPath);
		nRet = UGW_FAILURE;
		goto finish;
	}
	if ( wlObj == NULL) {
		LOGF_LOG_ERROR("ERROR Null Pointer WlObj, can't return data \n");
		nRet = UGW_FAILURE;
		goto finish;
	}

	obj = wlObj;
	while ( getline(&line, &len, fp) != -1 ) {
		if((len1=strnlen_s(line,128+256)) <= 0){ //param+obj
			LOGF_LOG_ERROR("fetchig len is failed\n");
			nRet = UGW_FAILURE;
			goto finish;
		}

		parameterName=strtok_s(line,&len1, "=", &pcSavePtr);
		parameterValue=strtok_s(NULL, &len1, "\n", &pcSavePtr);

		if (parameterName == NULL)
		{
			nRet = UGW_FAILURE;
			goto finish;	
		}

		if(parameterValue == NULL)
		{
			continue;
		}

		if ( ! strncmp(parameterName, "Object", strnlen_s("Object",10)) ) {
			objCurrentIdx =  indexFromString(parameterName);
			if ( objCurrentIdx < 0 ) {
				LOGF_LOG_ERROR("Illegal Object Index \n");
				nRet = UGW_FAILURE;
				goto finish;
			}

			//strcpy(objCurrentName, parameterValue);
			if (strncpy_s(objCurrentName, MAX_LEN_PARAM_NAME , parameterValue, MAX_LEN_PARAM_NAME) != EOK ) {
				LOGF_LOG_ERROR("strncpy_s failed \n");
				nRet = UGW_FAILURE;
				goto finish;
			}
			obj = help_addObjList(obj, objCurrentName, 0, 0, 0, 0);
			continue;
		}

		if (  indexFromString(parameterName) !=  objCurrentIdx ) {
			LOGF_LOG_ERROR("Parameter Index not equal to Object Index \n");
			nRet = UGW_FAILURE;
			goto finish;
		}
		
		HELP_EDIT_SELF_NODE(obj, objCurrentName, parameterName, parameterValue, 0, 0);
	}

finish:
	if (nFSt == EOK && fp != NULL)
		fclose(fp);
	if ( line )
		free(line);
	return nRet;
}


/*  =============================================================================
 *   Function Name 	: help_updateObjName					*
 *   Description 	: Function to update object incase object name doesnt 	*
 *			  end with "."
 *  ============================================================================*/
static void help_updateObjName( ObjList *pxObjSrc, ObjList *pxObjDst)
{
	ObjList *pxTmpObj;
	char sBuf[MAX_LEN_OBJNAME]={0};
	size_t nObjLen=0;

	FOR_EACH_OBJ(pxObjSrc, pxTmpObj)
	{
		sBuf[0]='\0';
		nObjLen = strnlen_s(pxTmpObj->sObjName,MAX_LEN_OBJNAME-1);
		if (nObjLen > 1 && nObjLen < MAX_LEN_OBJNAME)
	        {	
			if(pxTmpObj->sObjName[nObjLen-1] != '.')
			{
				if (sprintf_s(sBuf, MAX_LEN_OBJNAME, "%s.", pxTmpObj->sObjName) <=0) {
					LOGF_LOG_CRITICAL("sprintf_s failed\n");
				}
				if (sprintf_s(pxTmpObj->sObjName, MAX_LEN_OBJNAME, "%s", sBuf) <= 0) {
					LOGF_LOG_CRITICAL("sprintf_s failed\n");
				}
			}
		}else{
			LOGF_LOG_CRITICAL("Invalid Length %zu \n", nObjLen);
		}
	}

	FOR_EACH_OBJ(pxObjDst, pxTmpObj)
	{
		sBuf[0]='\0';
		nObjLen = strnlen_s(pxTmpObj->sObjName,MAX_LEN_OBJNAME);
		if (nObjLen > 1 && nObjLen < MAX_LEN_OBJNAME)
		{
			if(pxTmpObj->sObjName[nObjLen-1] != '.')
			{
				if (sprintf_s(sBuf, MAX_LEN_OBJNAME, "%s.", pxTmpObj->sObjName) <=0) {
					LOGF_LOG_CRITICAL("sprintf_s failed\n");
				}
				if (sprintf_s(pxTmpObj->sObjName, MAX_LEN_OBJNAME, "%s", sBuf) <= 0) {
					LOGF_LOG_CRITICAL("sprintf_s failed\n");
				}
			}
		}else{
			LOGF_LOG_CRITICAL("Invalid Length %zu \n", nObjLen);
		}
	}
}



/*  =============================================================================
 *   Function Name 	: help_addAttrObjList					*
 *   Description 	: Function to add attr object to the head node		*
 *  ============================================================================*/
OUT ObjAttrList *  help_addObjAttrList(IN ObjAttrList *pxObjList, IN const char *pcObjName, IN const char *pcWebName,IN uint32_t unFlag)           
{
	ObjAttrList *pxTmp;

	if(pcObjName == NULL){
		LOGF_LOG_ERROR("ObjectName Can't be empty\n");
		return NULL;
	}

	pxTmp = HELP_MALLOC(sizeof(ObjAttrList));
	if(!pxTmp) 
	{
		LOGF_LOG_CRITICAL(" malloc failed\n");
		return NULL;
	}

	if(pcObjName != NULL)
	{
		if (strncpy_s(pxTmp->sObjName,MAX_LEN_OBJNAME ,pcObjName, MAX_LEN_OBJNAME) != EOK) {
			LOGF_LOG_CRITICAL("strncpy_s failed\n");
		}
	}

	if(pcWebName != NULL)
	{
		if (strncpy_s(pxTmp->sObjWebName,MAX_LEN_WEBNAME, pcWebName, MAX_LEN_WEBNAME) != EOK) {
			LOGF_LOG_CRITICAL("strncpy_s failed\n");
		}
	}

	pxTmp->unObjAttr = unFlag;

	INIT_LIST_HEAD(&pxTmp->xParamAttrList.xPlist);
	list_add_tail( &(pxTmp->xOlist), &(pxObjList->xOlist) );
	return pxTmp;
}

/*  =============================================================================
 *   Function Name 	: help_addParamAttrList					*
 *   Description 	: Function to add parameter list to the head node	*
 *  ============================================================================*/
ParamAttrList * help_addParamAttrList(IN ObjAttrList *pxObjList,IN const char *pcParamName, IN const char *pcParamProfile, 
			  IN const char *pcParamWebName, IN const char *pcValidVal,IN const char *pcParamValue,
			  IN uint32_t unMinVal, IN uint32_t unMaxVal, IN uint32_t unMinLen, IN uint32_t unMaxLen,
			  IN uint32_t unParamFlag)
{
	ParamAttrList *pxTmp;

	if(pcParamName == NULL){
		LOGF_LOG_ERROR("ParameterName Can't be empty\n");
		return NULL;
	}

	pxTmp = HELP_MALLOC(sizeof(ParamAttrList));
	if(!pxTmp) 
	{
		LOGF_LOG_CRITICAL("malloc failed\n");
		return NULL;
	}
	
	if(pcParamName != NULL)
	{
		if (strncpy_s(pxTmp->sParamName, MAX_LEN_PARAM_NAME, pcParamName, MAX_LEN_PARAM_NAME) != EOK) {
			LOGF_LOG_CRITICAL("strncpy_s failed\n");
		}
	}
	
	if(pcParamProfile != NULL)
	{
		if (strncpy_s(pxTmp->sParamProfile, MAX_LEN_PROFILENAME , pcParamProfile, MAX_LEN_PROFILENAME) != EOK) {
			LOGF_LOG_CRITICAL("strncpy_s failed\n");
		}
	}
	
	if(pcParamWebName != NULL)
	{
		if (strncpy_s(pxTmp->sParamWebName, MAX_LEN_WEBNAME ,pcParamWebName, MAX_LEN_WEBNAME) !=  EOK) {
			LOGF_LOG_CRITICAL("strncpy_s failed\n");
		}
	}
	
	if(pcValidVal != NULL)
	{
		if (strncpy_s(pxTmp->sParamValidVal, MAX_LEN_VALID_VALUE, pcValidVal, MAX_LEN_VALID_VALUE) != EOK) {
			LOGF_LOG_CRITICAL("strncpy_s failed\n");
		}
	}
	
	if(pcParamValue != NULL)
	{
		if (strncpy_s(pxTmp->sParamValue, MAX_LEN_PARAM_VALUE, pcParamValue, MAX_LEN_PARAM_VALUE) != EOK) {
			LOGF_LOG_CRITICAL("strncpy_s failed\n");
		}
	}

	pxTmp->unMinVal  = unMinVal;
	pxTmp->unMaxVal  = unMaxVal;
	pxTmp->unMinLen  = unMinLen;
	pxTmp->unMaxLen  = unMaxLen;
	pxTmp->unParamFlag = unParamFlag;

	list_add_tail( &(pxTmp->xPlist), &(pxObjList->xParamAttrList.xPlist) );
	return pxTmp;
}

/*  =============================================================================
 *   Function Name 	: help_addAcsObjList					*
 *   Description 	: Function to add object to the head node		*
 *  ============================================================================*/
OUT ObjACSList *  help_addAcsObjList(IN ObjACSList *pxObjList, IN const char *pcObjName, IN uint32_t unObjOper, IN uint32_t unFlag)           
{
	ObjACSList *pxTmp;
	
	if(pcObjName == NULL){
		LOGF_LOG_CRITICAL("ObjectName can't be empty\n");
		return NULL;
	}

	pxTmp = HELP_MALLOC(sizeof(ObjACSList));
	if(!pxTmp) 
	{
		LOGF_LOG_CRITICAL(" malloc failed\n");
		return NULL;
	}

	if((pcObjName != NULL) && (strnlen_s(pcObjName,MAX_LEN_OBJNAME) < MAX_LEN_OBJNAME) )
	{
		memset(pxTmp->sObjName, 0x0, MAX_LEN_OBJNAME);
		if (strncpy_s(pxTmp->sObjName, MAX_LEN_OBJNAME, pcObjName, MAX_LEN_OBJNAME) != EOK) {
			LOGF_LOG_CRITICAL("strncpy_s failed\n");
		}
	}
	else
	{
		if(pcObjName != NULL)
		{
			LOGF_LOG_CRITICAL("Object Name Buffer OverFlow [%s]\n",pcObjName);
		}
		HELP_FREE(pxTmp);
		return NULL;
	}

	pxTmp->unObjOper = unObjOper; 
	pxTmp->unObjFlag = unFlag;

	INIT_LIST_HEAD(&pxTmp->xParamAcsList.xPlist);
	list_add_tail( &(pxTmp->xOlist), &(pxObjList->xOlist) );
	return pxTmp;
}

/*  =============================================================================
 *   Function Name 	: help_addAcsParamList					*
 *   Description 	: Function to add parameter list to the head node	*
 *  ============================================================================*/
OUT ParamACSList * help_addAcsParamList(IN ObjACSList *pxObjList, IN const char *pcParamName, 
								IN const char *pcParamValue,IN uint32_t unFlag)
{
	ParamACSList *pxTmp;
	pxTmp = HELP_MALLOC(sizeof(ParamACSList));
	if(!pxTmp) 
	{
		LOGF_LOG_CRITICAL("malloc failed\n");
		return NULL;
	}
	
	if( (pcParamName != NULL) && (strnlen_s(pcParamName,MAX_LEN_PARAM_NAME) < MAX_LEN_PARAM_NAME) )
	{
		memset(pxTmp->sParamName, 0x0, MAX_LEN_PARAM_NAME);
		if (strncpy_s(pxTmp->sParamName, MAX_LEN_PARAM_NAME, pcParamName, MAX_LEN_PARAM_NAME) != EOK ){
			LOGF_LOG_CRITICAL("strncpy_s failed\n");
		}
	}
	else
	{
		if(pcParamName != NULL)
		{
			LOGF_LOG_CRITICAL("Param Name Buffer OverFlow ParamName[%s]: ParamNameLen[%zu]\n",pcParamName,strnlen_s(pcParamName,MAX_LEN_PARAM_NAME));
		}
		HELP_FREE(pxTmp);
		return NULL;
	}

	if((pcParamValue != NULL) && (strnlen_s(pcParamValue,MAX_LEN_PARAM_VALUE) < MAX_LEN_PARAM_VALUE) )
	{
		memset(pxTmp->sParamValue, 0x0, MAX_LEN_PARAM_VALUE);
		if (strncpy_s(pxTmp->sParamValue, MAX_LEN_PARAM_VALUE, pcParamValue, MAX_LEN_PARAM_VALUE) != EOK ) {
			LOGF_LOG_CRITICAL("strncpy_s failed\n");
		}
	}
	else
	{
		if((pcParamValue != NULL))
		{
			LOGF_LOG_CRITICAL("Param Value Buffer OverFlow ParamName [%s] ParamValue[%s]: ParamValueLen[%zu]\n",pcParamName,pcParamValue,strnlen_s(pcParamValue,MAX_LEN_PARAM_VALUE));
			HELP_FREE(pxTmp);
			return NULL;
		}
	}
		
	pxTmp->unParamFlag=unFlag;

	list_add_tail( &(pxTmp->xPlist), &(pxObjList->xParamAcsList.xPlist) );
	
	return pxTmp;
}

/*  =============================================================================
 *   Function Name 	: help_addObjList					*
 *   Description 	: Function to add object to the head node		*
 *  ============================================================================*/
OUT ObjList *  help_addObjList(IN ObjList *pxObjList, IN const char *pcObjName, IN uint16_t unSid, IN uint16_t unOid, IN uint32_t unObjOper, IN uint32_t unFlag)           
{
	ObjList *pxTmp;

	if(pcObjName == NULL){
		LOGF_LOG_CRITICAL("ObjectName Can't be empty \n");
		return NULL;
	}

	pxTmp = HELP_MALLOC(sizeof(ObjList));
	if(!pxTmp) 
	{
		LOGF_LOG_CRITICAL(" malloc failed\n");
		return NULL;
	}

	if((pcObjName != NULL) && (strnlen_s(pcObjName,MAX_LEN_OBJNAME) < MAX_LEN_OBJNAME) )
	{
		memset(pxTmp->sObjName, 0x0, MAX_LEN_OBJNAME);
		if (strncpy_s(pxTmp->sObjName, MAX_LEN_OBJNAME, pcObjName, MAX_LEN_OBJNAME) != EOK) {
			LOGF_LOG_CRITICAL("strncpy_s failed\n");
		}
	}
	else
	{
		LOGF_LOG_CRITICAL("Object Name Buffer OverFlow [%s]\n",pcObjName);
		HELP_FREE(pxTmp);
		return NULL;
	}

	pxTmp->unSid = unSid;
	pxTmp->unOid = unOid;
	pxTmp->unObjOper = unObjOper; 
	pxTmp->unObjFlag = unFlag;

	INIT_LIST_HEAD(&pxTmp->xParamList.xPlist);
	list_add_tail( &(pxTmp->xOlist), &(pxObjList->xOlist) );
	return pxTmp;
}

/*  =============================================================================
 *   Function Name 	: help_addUCIConfigData					*
 *   Description 	: Function to add  UCIConfigDatato the head node	*
 *  ============================================================================*/
OUT UCIConfigData* help_addUCIConfigData(IN UCIConfigData *pxUCIConfigData)
{
	ParamList *__pxTmpConfParam = NULL;
	ParamList *__pxTmpValParam = NULL;
	ValueList *__pxTmpValueList = NULL;
	ArrayList *__pxTmpArrayList = NULL;
	UCIConfigData *pxTmp = NULL;

	pxTmp = HELP_MALLOC(sizeof(UCIConfigData));
	if(!pxTmp)
	{
		LOGF_LOG_CRITICAL(" malloc failed\n");
		return NULL;
	}

	list_add_tail( &(pxTmp->xClist), &(pxUCIConfigData->xClist) );

	__pxTmpConfParam = HELP_MALLOC(sizeof(ParamList));
	if(!__pxTmpConfParam)
	{
		LOGF_LOG_CRITICAL(" malloc failed\n");

		free(pxTmp);

		return NULL;
	}
	INIT_LIST_HEAD(&(__pxTmpConfParam->xPlist)); 
	pxTmp->pxConfigList = (void *)__pxTmpConfParam;

	__pxTmpValueList = HELP_MALLOC(sizeof(ValueList));
	if(!__pxTmpValueList)
	{
		LOGF_LOG_CRITICAL(" malloc failed\n");

		free(pxTmp);
		free(__pxTmpConfParam);

		return NULL;
	}
	pxTmp->pxValueList = (void *)__pxTmpValueList;

	__pxTmpValParam = HELP_MALLOC(sizeof(ParamList));
	if(!__pxTmpValParam)
	{
		LOGF_LOG_CRITICAL(" malloc failed\n");

		free(pxTmp);
		free(__pxTmpConfParam);
		free(__pxTmpValueList);

		return NULL;
	}
	INIT_LIST_HEAD(&(__pxTmpValParam->xPlist)); 
	pxTmp->pxValueList->pxParamValueList = (void *)__pxTmpValParam;

	__pxTmpArrayList = HELP_MALLOC(sizeof(ArrayList));
	if(!__pxTmpArrayList)
	{
		LOGF_LOG_CRITICAL(" malloc failed\n");
		free(pxTmp);
		free(__pxTmpConfParam);
		free(__pxTmpValueList);
		free(__pxTmpValParam);
		return NULL;
	}

	INIT_LIST_HEAD(&(__pxTmpArrayList->xClist));
	pxTmp->pxValueList->pxArrayValueList = (void *)__pxTmpArrayList;

	return pxTmp;
}

/*  =============================================================================
 *   Function Name 	: help_addParamList					*
 *   Description 	: Function to add parameter list to the head node	*
 *  ============================================================================*/
OUT ParamList * help_addParamList(IN ObjList *pxObjList,IN const char *pcParamName, IN uint16_t unParamId, 
			IN const char *pcParamValue,IN uint32_t unFlag)
{
	ParamList *pxTmp;
	
	if(pcParamName == NULL){
		LOGF_LOG_CRITICAL("Invalid ParameterName can't be empty\n");
		return NULL;
	}

	pxTmp = HELP_MALLOC(sizeof(ParamList));
	if(!pxTmp) 
	{
		LOGF_LOG_CRITICAL("malloc failed\n");
		return NULL;
	}
	
	if( (pcParamName != NULL) && (strnlen_s(pcParamName,MAX_LEN_PARAM_NAME) < MAX_LEN_PARAM_NAME) )
	{
		memset(pxTmp->sParamName, 0x0, MAX_LEN_PARAM_NAME);
		if (strncpy_s(pxTmp->sParamName, MAX_LEN_PARAM_NAME, pcParamName, MAX_LEN_PARAM_NAME) != EOK) {
			LOGF_LOG_CRITICAL("strncpy_s failed\n");
		}
	}
	else
	{
		if(pcParamName != NULL)
		{
			LOGF_LOG_CRITICAL("Param Name Buffer OverFlow ParamName[%s]: ParamNameLen[%zu]\n",pcParamName,strnlen_s(pcParamName,MAX_LEN_PARAM_NAME));
		}
		HELP_FREE(pxTmp);
		return NULL;
	}

	
	pxTmp->unParamId = unParamId;

	if((pcParamValue != NULL) && (strnlen_s(pcParamValue,MAX_LEN_PARAM_VALUE) < MAX_LEN_PARAM_VALUE) )
	{
		memset(pxTmp->sParamValue, 0x0, MAX_LEN_PARAM_VALUE);
		if (strncpy_s(pxTmp->sParamValue, MAX_LEN_PARAM_VALUE, pcParamValue, MAX_LEN_PARAM_VALUE) != EOK){
			LOGF_LOG_CRITICAL("strncpy_s failed\n");
		}
	}
	else
	{
		if((pcParamValue != NULL))
		{
			LOGF_LOG_CRITICAL("Param Value Buffer OverFlow ParamName [%s] ParamValue[%s]: ParamValueLen[%zu]\n",pcParamName,pcParamValue,strnlen_s(pcParamValue,MAX_LEN_PARAM_VALUE));
			HELP_FREE(pxTmp);
			return NULL;
		}
	}
		
	pxTmp->unParamFlag=unFlag;

	list_add_tail( &(pxTmp->xPlist), &(pxObjList->xParamList.xPlist) );
	
	return pxTmp;
}

/*  =============================================================================
 *   Function Name 	: help_paramListOnly					*
 *   Description 	: Function to add parameter list to the head node	*
 *  ============================================================================*/
ParamList * help_paramListOnly(IN ParamList *pxParamList,IN const char *pcParamName, IN uint16_t unParamId, 
			IN const char *pcParamValue,IN uint32_t unFlag)
{
	ParamList *pxTmp;

	if(pcParamName == NULL){
		LOGF_LOG_CRITICAL("Invalid ParameterName can't be empty\n");
		return NULL;
	}

	pxTmp = HELP_MALLOC(sizeof(ParamList));
	if(!pxTmp) 
	{
		LOGF_LOG_CRITICAL(" malloc failed\n");
		return NULL;
	}
	
	if((pcParamName != NULL) && (strnlen_s(pcParamName,MAX_LEN_PARAM_NAME) < MAX_LEN_PARAM_NAME) )
	{
		memset(pxTmp->sParamName, 0x0, MAX_LEN_PARAM_NAME);
		if(strncpy_s(pxTmp->sParamName, MAX_LEN_PARAM_NAME, pcParamName, MAX_LEN_PARAM_NAME) != EOK) {
			LOGF_LOG_CRITICAL("strncpy_s failed\n");
		}
	}
	else
	{
		if(pcParamName != NULL)
		{
			LOGF_LOG_CRITICAL(" Param Name Buffer OverFlow [%s] \n",pcParamName);
		}
		HELP_FREE(pxTmp);
		return NULL;
	}
	
	pxTmp->unParamId = unParamId;

	if((pcParamValue != NULL) && (strnlen_s(pcParamValue,MAX_LEN_PARAM_VALUE) < MAX_LEN_PARAM_VALUE) )
	{
		memset(pxTmp->sParamValue, 0x0, MAX_LEN_PARAM_VALUE);
		if (strncpy_s(pxTmp->sParamValue, MAX_LEN_PARAM_VALUE, pcParamValue, MAX_LEN_PARAM_VALUE) != EOK) {
			LOGF_LOG_CRITICAL("strncpy_s failed\n");
		}
	}
	else
	{
		if((pcParamValue != NULL))
		{
			LOGF_LOG_CRITICAL("Param Value Buffer OverFlow ParamName [%s] ParamValue[%s]: ParamValueLen[%zu]\n",pcParamName,pcParamValue,strnlen_s(pcParamValue,MAX_LEN_PARAM_VALUE));
			HELP_FREE(pxTmp);
			return NULL;
		}
	}

	pxTmp->unParamFlag=unFlag;

	list_add_tail( &(pxTmp->xPlist), &(pxParamList->xPlist) );
	return pxTmp;
}

/*  =============================================================================
 *   Function Name 	: help_printObj						*
 *   Description 	: Function to traverse list and dump object name,id	*
 *  ============================================================================*/
void help_printObj(IN void *pxObj,uint32_t unSubOper)
{
	if (IS_OBJLIST(unSubOper))
	{
		ObjList *pxTmpObj;
		ParamList *pxParam;
		
		fprintf(stderr,"\n @@@ PRINT OBJLIST START @@@\n");
		
		FOR_EACH_OBJ(pxObj,pxTmpObj)
		{
			fprintf(stderr,"\"%s\": { \n",GET_OBJ_NAME(pxTmpObj));
			fprintf(stderr,"\t\t \"Sid\" :\"%d\",\n",GET_OBJ_SID(pxTmpObj));
			fprintf(stderr,"\t\t \"Oid\" : \"%d\",\n", GET_OBJ_OID(pxTmpObj));

			if (IS_SOPT_OBJNAME(unSubOper))
			{
				fprintf(stderr,"\t\t \"ObjOper\" : \"%d\"\n",GET_OBJ_SUBOPER(pxTmpObj));
			}
			else
			{
				fprintf(stderr,"\t\t \"ObjOper\" : \"%d\",\n",GET_OBJ_SUBOPER(pxTmpObj));
			}
			
			fprintf(stderr,"\t\t \"Flag\" : \"0x%x\",\n",GET_OBJ_FLAG(pxTmpObj));

			FOR_EACH_PARAM(pxTmpObj,pxParam)
			{
				
				fprintf(stderr,"\t\t \"%s\": { \n",GET_PARAM_NAME(pxParam));
				fprintf(stderr,"\t\t\t \"paramId\" : \"%d\",\n",GET_PARAM_ID(pxParam)); 
				fprintf(stderr,"\t\t\t \"paramValue\" : \"%s\",\n",GET_PARAM_VALUE(pxParam)); 
				fprintf(stderr,"\t\t\t \"paramFlag\" : \"0x%x\"\n",GET_PARAM_FLAG(pxParam)); 
				fprintf(stderr,"\t\t },\n");
			}
			fprintf(stderr,"\t },\n");
		}
		fprintf(stderr," }\n");
		fprintf(stderr,"\n @@@@@@ PRINT OBJLIST END @@@@@@\n");
	}
	else if (IS_SOPT_OBJATTR(unSubOper))
	{
		ObjAttrList *pxTmpAttrObj;
		ParamAttrList *pxAttrParam;
			
		FOR_EACH_OBJATTR(pxObj,pxTmpAttrObj)
		{
			fprintf(stderr,"\t \"%s\": { \n",GET_ATTR_OBJNAME(pxTmpAttrObj));
			fprintf(stderr,"\t\t \"WebName\" :\"%s\",\n",GET_ATTR_WEBNAME(pxTmpAttrObj));
			fprintf(stderr,"\t\t \"ObjFlag\" : \"0x%x\",\n",GET_ATTR_FLAG(pxTmpAttrObj));
			FOR_EACH_PARAM_ATTR(pxTmpAttrObj,pxAttrParam)
			{
				fprintf(stderr,"\t\t \"%s\": { \n",GET_ATTR_PARAMNAME(pxAttrParam));
				fprintf(stderr,"\t\t\t \"paramProfile\" : \"%s\",\n",GET_ATTR_PARAMPROFILE(pxAttrParam)); 
				fprintf(stderr,"\t\t\t \"paramWebName\" : \"%s\",\n",GET_ATTR_PARAMWEBNAME(pxAttrParam)); 
				fprintf(stderr,"\t\t\t \"paramValidVal\" : \"%s\",\n",pxAttrParam->sParamValidVal);
				fprintf(stderr,"\t\t\t \"paramValue\" : \"%s\",\n",GET_ATTR_PARAMVALUE(pxAttrParam)); 
				fprintf(stderr,"\t\t\t \"paramMinVal\" : \"%d\",\n",GET_ATTR_MINVAL(pxAttrParam));
				fprintf(stderr,"\t\t\t \"paramMaxVal\" : \"%d\",\n",GET_ATTR_MAXVAL(pxAttrParam));
				fprintf(stderr,"\t\t\t \"paramMinLen\" : \"%d\",\n",GET_ATTR_MINLEN(pxAttrParam));
				fprintf(stderr,"\t\t\t \"paramMaxLen\" : \"%d\",\n",GET_ATTR_MAXLEN(pxAttrParam));
				fprintf(stderr,"\t\t\t \"paramFlag\" : \"0x%x\",\n",GET_ATTR_PARAMFLAG(pxAttrParam));
				fprintf(stderr,"\t\t },\n");
			}
			fprintf(stderr,"\t },\n");
		}
		fprintf(stderr,"}\n");
	}
	else if (IS_SOPT_OBJACSATTR(unSubOper))
	{
		ObjACSList *pxTmpAcsObj;
		ParamACSList *pxAcsParam;
			
		FOR_EACH_OBJ_ACS_ATTR(pxObj,pxTmpAcsObj)
		{
			fprintf(stderr,"\t \"%s\": { \n",GET_ACS_OBJNAME(pxTmpAcsObj));
			fprintf(stderr,"\t\t \"ObjOper\" :\"%d\",\n",GET_ACS_OBJOPER(pxTmpAcsObj));
			fprintf(stderr,"\t\t \"ObjFlag\" : \"0x%x\",\n",GET_ACS_OBJFLAG(pxTmpAcsObj));
			FOR_EACH_PARAM_ACS_ATTR(pxTmpAcsObj,pxAcsParam)
			{
				fprintf(stderr,"\t\t \"%s\": { \n",GET_ACS_PARAMNAME(pxAcsParam));
				fprintf(stderr,"\t\t\t \"AccessList\" : \"%s\",\n",GET_ACS_ACCESSLIST(pxAcsParam)); 
				if(IS_ATTR_ACTIVE_NOTIFY_SET(GET_ACS_PARAMFLAG(pxAcsParam)))
				{
					fprintf(stderr,"\t\t\t \"Notification\" : \"Active\",\n"); 
				}
				else if(IS_ATTR_PASSIVE_NOTIFY_SET(GET_ACS_PARAMFLAG(pxAcsParam)))
				{
					fprintf(stderr,"\t\t\t \"Notification\" : \"Passive\",\n"); 
				}
				else
				{
					fprintf(stderr,"\t\t\t \"Notification\" : \"Disabled\",\n"); 
				}
				fprintf(stderr,"\t\t\t \"paramFlag\" : \"%d\",\n",pxAcsParam->unParamFlag); 
				fprintf(stderr,"\t\t },\n");
			}
			fprintf(stderr,"\t },\n");
		}
		fprintf(stderr,"}\n");
	}
}

/*  =============================================================================
 *   Function Name 	: help_printParamList					*
 *   Description 	: Function to traverse list and dump paramlist  	*
 *  ============================================================================*/
void help_printParamList(IN ParamList *pxParamList)
{
	ParamList *pxParam;
	fprintf(stderr,"{\n");	
	FOR_EACH_PARAM_ONLY(pxParamList,pxParam)
	{
		fprintf(stderr,"\t\t \"%s\": { \n",GET_PARAM_NAME(pxParam));
		fprintf(stderr,"\t\t\t \"paramId\" : \"%d\",\n",(int16_t)GET_PARAM_ID(pxParam)); 
		fprintf(stderr,"\t\t\t \"paramValue\" : \"%s\",\n",GET_PARAM_VALUE(pxParam)); 
		fprintf(stderr,"\t\t\t \"paramFlag\" : \"%d\"\n",GET_PARAM_FLAG(pxParam)); 
		fprintf(stderr,"\t\t }\n");
	}
	fprintf(stderr,"}\n");
}

/*  =============================================================================
 *   Function Name 	: help_printUCIConfigData				*
 *   Description 	: Function to traverse list and dump UCIConfigData	*
 *  ============================================================================*/
void help_printUCIConfigData(IN UCIConfigData *pxUCIConfigData)
{
	UCIConfigData *pxTempConf = NULL;
	ParamList *pxParam = NULL;
	ArrayList *pxArrayValueListTmp = NULL;

	fprintf(stderr,"\n @@@@@@ PRINT UCIConfigData START @@@@@@\n");
	fprintf(stderr," {\n");

	FOR_EACH_CONFIG(pxUCIConfigData, pxTempConf) {
		fprintf(stderr,"\t {\n");

		if (pxTempConf->pxConfigList != NULL) {
			FOR_EACH_PARAM_ONLY(pxTempConf->pxConfigList, pxParam) {
				fprintf(stderr,"\t\t \"%s\" : \"%s\",\n",GET_PARAM_NAME(pxParam), GET_PARAM_VALUE(pxParam));
			}
		}

		if (pxTempConf->pxValueList->pxParamValueList != NULL) {
			FOR_EACH_PARAM_ONLY(pxTempConf->pxValueList->pxParamValueList, pxParam) {
				fprintf(stderr,"\t\t \"%s\" : \"%s\",\n",GET_PARAM_NAME(pxParam), GET_PARAM_VALUE(pxParam));
			}
		}

		if (pxTempConf->pxValueList->pxArrayValueList != NULL) {
			FOR_EACH_ARRAYLIST(pxTempConf->pxValueList->pxArrayValueList, pxArrayValueListTmp) {
				if (pxArrayValueListTmp->pxArrayValues != NULL) {
					fprintf(stderr,"\t\t \"%s\": [\n", pxArrayValueListTmp->sArrayName);
					FOR_EACH_PARAM_ONLY(pxArrayValueListTmp->pxArrayValues, pxParam) {
						fprintf(stderr,"\t\t\t \"%s\",\n", GET_PARAM_VALUE(pxParam));
					}
					fprintf(stderr,"\t\t ],\n");
				}
			}
		}
		fprintf(stderr,"\t },\n");
	}

	fprintf(stderr," }\n");
	fprintf(stderr,"\n @@@@@@ PRINT UCIConfigData END @@@@@@\n");

	return;
}

/*  =============================================================================
 *   Function Name 	: help_delCurObj						*
 *   Description 	: Function to traverse list and free objects 		*
 *  ============================================================================*/
int help_delCurObj(IN void *pObj, const char * pcObjName, IN uint32_t unSubOper)
{
	if (IS_OBJLIST(unSubOper))
	{
		ObjList *pxTempObj; 
		ParamList *pxParamList; 
		ObjList *pxObjList;

		pxObjList = pObj;
		while( !list_empty(&pxObjList->xOlist) ) 
		{
			if (strcmp(pxObjList->sObjName,pcObjName) == 0)
			{ 
				pxTempObj = pxObjList;
				while( !list_empty(&pxTempObj->xParamList.xPlist) ) 
				{
					pxParamList = list_entry(pxTempObj->xParamList.xPlist.next,ParamList,xPlist);
					list_del(&pxParamList->xPlist);
					free(pxParamList);
					pxParamList = NULL;
				}
				list_del(&pxTempObj->xOlist);
				free(pxTempObj);
				pxTempObj = NULL;
				return UGW_SUCCESS;
			}
			else
			{
				pxObjList = list_entry(pxObjList->xOlist.next,ObjList,xOlist);
			} 
		}
	}
	else if (IS_SOPT_OBJATTR(unSubOper))
	{
		ObjAttrList *pxTempAttrObj; 
		ParamAttrList *pxParamAttrList; 
		ObjAttrList *pxObjAttrList;

		pxObjAttrList = pObj;
		while( !list_empty(&pxObjAttrList->xOlist) ) 
		{
			if (strcmp(pxObjAttrList->sObjName,pcObjName) == 0)
			{
				pxTempAttrObj = list_entry(pxObjAttrList->xOlist.next,ObjAttrList,xOlist);
				while( !list_empty(&pxTempAttrObj->xParamAttrList.xPlist) ) 
				{
					pxParamAttrList = list_entry(pxTempAttrObj->xParamAttrList.xPlist.next,ParamAttrList,xPlist);
					list_del(&pxParamAttrList->xPlist);
					free(pxParamAttrList);
					pxParamAttrList = NULL;
				}
				list_del(&pxTempAttrObj->xOlist);
				free(pxTempAttrObj);
				pxTempAttrObj = NULL;
				return UGW_SUCCESS;
			}
		}
	}
return UGW_SUCCESS;
}

/*  =============================================================================
 *   Function Name 	: help_delObj						*
 *   Description 	: Function to traverse list and free objects 		*
 *  ============================================================================*/
void help_delObj(IN void *pObj, IN uint32_t unSubOper,IN uint32_t unFlag)
{
	if (IS_OBJLIST(unSubOper))
	{
		ObjList *pxTempObj; 
		ParamList *pxParamList; 
		ObjList *pxObjList;

		pxObjList = pObj;
		while( !list_empty(&pxObjList->xOlist) ) 
		{
			pxTempObj = list_entry(pxObjList->xOlist.next,ObjList,xOlist);
			while( !list_empty(&pxTempObj->xParamList.xPlist) ) 
			{
				pxParamList = list_entry(pxTempObj->xParamList.xPlist.next,ParamList,xPlist);
				list_del(&pxParamList->xPlist);
				free(pxParamList);
				pxParamList = NULL;
			}
			list_del(&pxTempObj->xOlist);
			free(pxTempObj);
			pxTempObj = NULL;
		}
	}
	else if (IS_SOPT_OBJATTR(unSubOper))
	{
		ObjAttrList *pxTempAttrObj; 
		ParamAttrList *pxParamAttrList; 
		ObjAttrList *pxObjAttrList;

		pxObjAttrList = pObj;
		while( !list_empty(&pxObjAttrList->xOlist) ) 
		{
			pxTempAttrObj = list_entry(pxObjAttrList->xOlist.next,ObjAttrList,xOlist);
			while( !list_empty(&pxTempAttrObj->xParamAttrList.xPlist) ) 
			{
				pxParamAttrList = list_entry(pxTempAttrObj->xParamAttrList.xPlist.next,ParamAttrList,xPlist);
				list_del(&pxParamAttrList->xPlist);
				free(pxParamAttrList);
				pxParamAttrList = NULL;
			}
			list_del(&pxTempAttrObj->xOlist);
			free(pxTempAttrObj);
			pxTempAttrObj = NULL;
		}
	}
	else if (IS_SOPT_OBJACSATTR(unSubOper))
	{
		ObjACSList *pxTempAcsObj; 
		ParamACSList *pxParamAcsList; 
		ObjACSList *pxObjAcsList;

		pxObjAcsList = pObj;
		while( !list_empty(&pxObjAcsList->xOlist) ) 
		{
			pxTempAcsObj = list_entry(pxObjAcsList->xOlist.next,ObjACSList,xOlist);
			while( !list_empty(&pxTempAcsObj->xParamAcsList.xPlist) ) 
			{
				pxParamAcsList = list_entry(pxTempAcsObj->xParamAcsList.xPlist.next,ParamACSList,xPlist);
				list_del(&pxParamAcsList->xPlist);
				free(pxParamAcsList);
				pxParamAcsList = NULL;
			}
			list_del(&pxTempAcsObj->xOlist);
			free(pxTempAcsObj);
			pxTempAcsObj = NULL;
		}
	}

	if(unFlag == FREE_OBJLIST)
	{
		if (pObj != NULL)
		{
			free(pObj);
		}
	}
}


/*  =============================================================================
 *   Function Name 	: help_delConf						*
 *   Description 	: Function to traverse list and free  pxUCIConfigData	*
 *  ============================================================================*/
void help_delConf(IN void *pxUCIConfigData)
{
	bool bEmptyList = 1;
	UCIConfigData *pxTempConf = NULL;
	ParamList *pxParamList = NULL;
	ArrayList *pxArrayList = NULL;
	ArrayList *pxArrayValueListTmp = NULL;
	UCIConfigData *pxConfList = NULL;

	pxConfList = pxUCIConfigData;
	while( !list_empty(&pxConfList->xClist) )
	{
		bEmptyList = 0;
		pxTempConf = list_entry(pxConfList->xClist.next, UCIConfigData, xClist);

		if (pxTempConf->pxConfigList != NULL) {
			while( !list_empty(&pxTempConf->pxConfigList->xPlist) )
			{
				pxParamList = list_entry(pxTempConf->pxConfigList->xPlist.next, ParamList, xPlist);
				list_del(&pxParamList->xPlist);
				free(pxParamList);
				pxParamList = NULL;
			}
		}


		if (pxTempConf->pxValueList->pxArrayValueList != NULL) {
			FOR_EACH_ARRAYLIST(pxTempConf->pxValueList->pxArrayValueList, pxArrayValueListTmp) {
				if (pxArrayValueListTmp->pxArrayValues != NULL) {
					while( !list_empty(&pxArrayValueListTmp->pxArrayValues->xPlist) )
					{
						pxParamList = list_entry(pxArrayValueListTmp->pxArrayValues->xPlist.next, ParamList, xPlist);
						list_del(&pxParamList->xPlist);
						free(pxParamList);
						pxParamList = NULL;
					}
				}
			}

			while( !list_empty(&pxTempConf->pxValueList->pxArrayValueList->xClist) )
			{
				pxArrayList = list_entry(pxTempConf->pxValueList->pxArrayValueList->xClist.next, ArrayList, xClist);
				list_del(&pxArrayList->xClist);
				free(pxArrayList);
				pxArrayList = NULL;
			}
		}

		if (pxTempConf->pxValueList->pxParamValueList != NULL) {
			while( !list_empty(&pxTempConf->pxValueList->pxParamValueList->xPlist) )
			{
				pxParamList = list_entry(pxTempConf->pxValueList->pxParamValueList->xPlist.next, ParamList, xPlist);
				list_del(&pxParamList->xPlist);
				free(pxParamList);
				pxParamList = NULL;
			}
		}

		list_del(&pxTempConf->xClist);
		free(pxTempConf);
		pxTempConf = NULL;
	}

	if ((pxUCIConfigData != NULL) && (!bEmptyList))
	{
		free(pxUCIConfigData);
	}
}

/*  =============================================================================
 *   Function Name 	: help_delParam						*
 *   Description 	: Function to traverse Param list and free objects 	*
 *  ============================================================================*/
void help_delParam(IN void *pParam, IN uint32_t unFlag)
{
	if (IS_OBJLIST(unFlag))
	{
		ParamList *pxParamList; 
		ParamList *pxTmpParamList; 

		pxParamList = pParam;
		while( !list_empty(&pxParamList->xPlist) ) 
		{
			pxTmpParamList = list_entry(pxParamList->xPlist.next,ParamList,xPlist);
			list_del(&pxTmpParamList->xPlist);
			free(pxTmpParamList);
			pxTmpParamList = NULL;
		}
	}
	if (pParam != NULL)
	{
		free(pParam);
	}
}

/*  =============================================================================
 *   Function Name 	: help_copyObjList					*
 *   Description 	: copy current objlist to new objlist			*
 *  ============================================================================*/
void help_copyObjList(IN void *pDst , IN uint32_t unFlag , OUT void *pSrc)
{
	if (IS_OBJLIST(unFlag))
	{
		void *pxTObj;
		ParamList *pxParam;
		ObjList *pxSrcObj;
		pxSrcObj = pSrc;
		pxTObj = help_addObjList(pDst,GET_OBJ_NAME(pxSrcObj),
					     GET_OBJ_SID(pxSrcObj),
					     GET_OBJ_OID(pxSrcObj),
					     GET_OBJ_SUBOPER(pxSrcObj),
					     GET_OBJ_FLAG(pxSrcObj));
		FOR_EACH_PARAM(pxSrcObj,pxParam)
		{
			help_addParamList(pxTObj,GET_PARAM_NAME(pxParam),
						GET_PARAM_ID(pxParam),
						GET_PARAM_VALUE(pxParam),
						GET_PARAM_FLAG(pxParam));
		}
	}
	else if (IS_SOPT_OBJATTR(unFlag))
	{
		void *pxTObj;
		ParamAttrList *pxParam;
		ObjAttrList *pxSrcObj;
		pxSrcObj = pSrc;
		pxTObj = help_addObjAttrList(pDst,GET_ATTR_OBJNAME(pxSrcObj),
						 GET_ATTR_WEBNAME(pxSrcObj),
						 GET_ATTR_FLAG(pxSrcObj));
		FOR_EACH_PARAM_ATTR(pxSrcObj,pxParam)
		{
			help_addParamAttrList(pxTObj,GET_ATTR_PARAMNAME(pxParam),
						    GET_ATTR_PARAMPROFILE(pxParam),
						    GET_ATTR_PARAMWEBNAME(pxParam),
						    GET_ATTR_PARAMVALUE(pxParam),
						    pxParam->sParamValidVal,
						    GET_ATTR_MINVAL(pxParam),
						    GET_ATTR_MAXVAL(pxParam),
						    GET_ATTR_MINLEN(pxParam),
						    GET_ATTR_MAXLEN(pxParam),
						    GET_ATTR_PARAMFLAG(pxParam));
		}
	}
	else if (IS_SOPT_OBJACSATTR(unFlag))
	{
		void *pxTObj;
                ParamACSList *pxParam;
                ObjACSList *pxSrcObj;
                pxSrcObj = pSrc;	

		pxTObj = help_addAcsObjList(pDst, GET_ACS_OBJNAME(pxSrcObj), GET_ACS_OBJOPER(pxSrcObj), GET_ACS_OBJFLAG(pxSrcObj));
		FOR_EACH_PARAM_ACS_ATTR(pxSrcObj, pxParam)
		{
			help_addAcsParamList(pxTObj, GET_ACS_PARAMNAME(pxParam), GET_ACS_ACCESSLIST(pxParam), GET_ACS_PARAMFLAG(pxParam));
		}
	}
}

/*  =============================================================================
 *   Function Name 	: help_copyObj						*
 *   Description 	: Function to copy object list as is to tmp objlist 	*
 *  ============================================================================*/
static void help_copyObj(ObjList *pxDst,ObjList *pxSrc)
{

	 ObjList *pxTmpObj=NULL,*pxObj=NULL;
	 ParamList *pxParam;
	 list_for_each_entry(pxTmpObj,&(pxSrc->xOlist),xOlist)		 
	 {
		 pxObj = help_addObjList(pxDst,pxTmpObj->sObjName,pxTmpObj->unSid,pxTmpObj->unOid,pxTmpObj->unObjOper,pxTmpObj->unObjFlag);
		 list_for_each_entry(pxParam,&(pxTmpObj->xParamList.xPlist),xPlist)
		 {
			 help_addParamList(pxObj,pxParam->sParamName,pxParam->unParamId,pxParam->sParamValue,pxParam->unParamFlag);
		 }
	 }
}


/*  =============================================================================
 *   Function Name 	: help_addParam						*
 *   Description 	: Function to add parameter list to the parameter node	*
 *  ============================================================================*/
static int help_addParam(IN ParamList *pxParam, IN const char *pcParamName, IN uint16_t unParamId, 
			IN const char *pcParamValue, IN uint32_t unFlag)
{
	ParamList *pxTmp;

	if(pcParamName == NULL){
		LOGF_LOG_CRITICAL("Invalid ParameterName can't be empty\n");
		return UGW_FAILURE;
	}

	pxTmp = HELP_MALLOC(sizeof(ParamList));
	if(!pxTmp) 
	{
		LOGF_LOG_CRITICAL("malloc failed\n");
		return ERR_MEMORY_ALLOC_FAILED;
	}
	
	if((pcParamName != NULL) && (strnlen_s(pcParamName,MAX_LEN_PARAM_NAME) < MAX_LEN_PARAM_NAME) )
	{
		if (strncpy_s(pxTmp->sParamName, MAX_LEN_PARAM_NAME, pcParamName, MAX_LEN_PARAM_NAME) != EOK) {
			LOGF_LOG_CRITICAL("strncpy_s failed\n");
		}
	}
	else
	{
		if(pcParamName != NULL)
		{
			LOGF_LOG_CRITICAL("Param Name Buffer OverFlow\n");
		}
		HELP_FREE(pxTmp);
		return UGW_FAILURE;
	}
	
	pxTmp->unParamId = unParamId;
	
	if((pcParamValue != NULL) && (strnlen_s(pcParamValue,MAX_LEN_PARAM_VALUE) < MAX_LEN_PARAM_VALUE) )
	{
		if (strncpy_s(pxTmp->sParamValue, MAX_LEN_PARAM_VALUE, pcParamValue, MAX_LEN_PARAM_VALUE) != EOK) {
			LOGF_LOG_CRITICAL("strncpy_s failed\n");
		}
	}
	else
	{
		if((pcParamValue != NULL))
		{
			LOGF_LOG_CRITICAL("Param value Buffer OverFlow ParamName[%s] ParamValue[%s] ParamLen[%zu]\n",pcParamName, pcParamValue, strnlen_s(pcParamValue,MAX_LEN_PARAM_VALUE));
			HELP_FREE(pxTmp);
			return UGW_FAILURE;
		}
	}
		
	pxTmp->unParamFlag=unFlag;

	list_add_tail( &(pxTmp->xPlist), &(pxParam->xPlist) );
	
	return UGW_SUCCESS;
}

/*  =============================================================================
 *   Function Name 	: help_copyParam						*
 *   Description 	: Function to copy param list as is to another paramlist*
 *  ============================================================================*/
void help_copyParamList(ParamList *pxDst,ParamList *pxSrc)
{
	 ParamList *pxParam;
	 list_for_each_entry(pxParam,&(pxSrc->xPlist),xPlist)		 
	 {
		help_addParam(pxDst,pxParam->sParamName,pxParam->unParamId,pxParam->sParamValue,pxParam->unParamFlag);
	 }
}

/*  =============================================================================
 *   Function Name 	: help_copyObjAttr					*
 *   Description 	: Function to copy object list as is to tmp objlist 	*
 *  ============================================================================*/
static void help_copyObjAttr(ObjAttrList *pxDst,ObjAttrList *pxSrc)
{

	 ObjAttrList *pxTmpObj=NULL,*pxObj=NULL;
	 ParamAttrList *pxParam;
	 FOR_EACH_OBJATTR(pxSrc,pxTmpObj)
	 {
		 pxObj = help_addObjAttrList(pxDst,pxTmpObj->sObjName,pxTmpObj->sObjWebName,pxTmpObj->unObjAttr);
		 FOR_EACH_PARAM_ATTR(pxTmpObj,pxParam)
		 {
			 help_addParamAttrList(pxObj,pxParam->sParamName,pxParam->sParamProfile,pxParam->sParamWebName,
					 				pxParam->sParamValidVal,pxParam->sParamValue,pxParam->unMinVal,
									pxParam->unMaxVal,pxParam->unMinLen,pxParam->unMaxLen,pxParam->unParamFlag);
		 }
	 }
}

/*  =============================================================================
 *   Function Name 	: help_copyAcsObj					*
 *   Description 	: Function to copy acs object list as is to tmp objlist *
 *  ============================================================================*/
static void help_copyAcsObj(ObjACSList *pxDst, ObjACSList *pxSrc)
{

	 ObjACSList *pxTmpObj=NULL,*pxObj=NULL;
	 ParamACSList *pxParam;
	 FOR_EACH_OBJ_ACS_ATTR(pxSrc,pxTmpObj)
	 {
		 pxObj = help_addAcsObjList(pxDst,pxTmpObj->sObjName,pxTmpObj->unObjOper,pxTmpObj->unObjFlag);
		 FOR_EACH_PARAM_ACS_ATTR(pxTmpObj,pxParam)
		 {
			 help_addAcsParamList(pxObj,pxParam->sParamName, pxParam->sParamValue, pxParam->unParamFlag);
		 }
	 }
}
/*  =============================================================================
 *   Function Name 	: help_copyCompelteObjList				*
 *   Description 	: Function to copy object list as is to tmp objlist 	*
 *  ============================================================================*/
void help_copyCompleteObjList(IN void *pDst , IN uint32_t unFlag , OUT void *pSrc)
{
	if (IS_OBJLIST(unFlag))
	{
		help_copyObj(pDst, pSrc);
	}
	else if (IS_SOPT_OBJATTR(unFlag))
	{
		help_copyObjAttr(pDst, pSrc);
	}
	else if (IS_SOPT_OBJACSATTR(unFlag))
	{	
		help_copyAcsObj(pDst, pSrc);
	}
}

/*  =============================================================================
 *   Function Name 	: help_editNode						* 
 *   Description 	: Function to update node in given objlist		*
 *  ============================================================================*/
uint32_t help_editNode (INOUT ObjList *pxDstObjList, IN char *pcObjName, IN char *pcParamName, IN char *pcParamValue, 
									IN uint32_t uParamId, IN uint32_t uParamFlag)
{
 	ObjList *pxTmpObjDst;
	ParamList *pxParam;
	uint32_t nRet = UGW_FAILURE;

	FOR_EACH_OBJ(pxDstObjList,pxTmpObjDst)
	{
		nRet = UGW_FAILURE;
		if ( (strncmp(pxTmpObjDst->sObjName,pcObjName,strnlen_s(pxTmpObjDst->sObjName,MAX_LEN_OBJNAME)) == 0) && (strnlen_s(pcObjName,MAX_LEN_OBJNAME) == strnlen_s(pxTmpObjDst->sObjName,MAX_LEN_OBJNAME)) )
                {
			FOR_EACH_PARAM(pxTmpObjDst,pxParam)
			{
				if ( (strcmp(pxParam->sParamName,pcParamName) == 0 ) && (strnlen_s(pxParam->sParamName,MAX_LEN_PARAM_NAME) == strnlen_s(pcParamName,MAX_LEN_PARAM_NAME)))
				{
					if(pcParamValue != NULL)
					{
						/* Tmp fix to cover SL issues*/
						if (pxParam->sParamValue == pcParamValue)
						{
							LOGF_LOG_INFO("Input and output ptrs are the same. Skipping update\n");
							nRet = UGW_SUCCESS;
						}
						else if(strnlen_s(pcParamValue,MAX_LEN_PARAM_VALUE) < MAX_LEN_PARAM_VALUE )
						{
							if (strncpy_s(pxParam->sParamValue,MAX_LEN_PARAM_VALUE, pcParamValue, MAX_LEN_PARAM_VALUE) != EOK) {
								LOGF_LOG_CRITICAL("strncpy_s failed\n");
							}
							nRet = UGW_SUCCESS;	
						}
						else
						{
							LOGF_LOG_CRITICAL("Buffer OverFlow \n");
							return UGW_FAILURE;
						}
					}
				}
			}
			if (nRet != UGW_SUCCESS)
			{
				/* parameter not exists add to the list. */
				help_addParamList(pxTmpObjDst,pcParamName,uParamId,pcParamValue,uParamFlag);
				nRet = UGW_SUCCESS;	

			}
		goto finish;
		}
	}
finish:
	return nRet;
}

/*  =============================================================================
 *   Function Name 	: help_editSelfNode					*
 *   Description 	: Function to edit extarcted from for_each and addobj	*
 *  ============================================================================*/
uint32_t help_editSelfNode(INOUT ObjList *pxDstObjList, IN char *pcObjName, IN char *pcParamName, 
				IN char *pcParamValue, IN uint32_t uParamId, IN uint32_t uParamFlag)
{
	ParamList *pxParam;
	uint32_t nRet = UGW_FAILURE;
	if( (strcmp(pxDstObjList->sObjName, pcObjName) == 0) && (strnlen_s(pxDstObjList->sObjName,MAX_LEN_OBJNAME) == strnlen_s(pcObjName,MAX_LEN_OBJNAME)) )
	{
		FOR_EACH_PARAM(pxDstObjList,pxParam)
		{
			if ( (strcmp(pxParam->sParamName,pcParamName) == 0 ) && (strnlen_s(pxParam->sParamName,MAX_LEN_PARAM_NAME) == strnlen_s(pcParamName,MAX_LEN_PARAM_NAME)))
			{
				if(pcParamValue != NULL)
				{
					/* Tmp fix to cover SL issues*/
					if (pxParam->sParamValue == pcParamValue)
					{
						LOGF_LOG_INFO("Input and output ptrs are the same. Skipping update\n");
						nRet = UGW_SUCCESS;
					}
					else if(strnlen_s(pcParamValue,MAX_LEN_PARAM_VALUE) < MAX_LEN_PARAM_VALUE)
					{
						if (strncpy_s(pxParam->sParamValue,MAX_LEN_PARAM_VALUE, pcParamValue, MAX_LEN_PARAM_VALUE) != EOK) {
							LOGF_LOG_CRITICAL("strncpy_s failed\n");
						}
						nRet = UGW_SUCCESS;	
					}
					else
					{
						LOGF_LOG_CRITICAL("Buffer OverFlow \n");
						return UGW_FAILURE;
					}
				}
			}
		}
		if (nRet != UGW_SUCCESS)
		{
			/* parameter not exists add to the list. */
			help_addParamList(pxDstObjList,pcParamName,uParamId,pcParamValue,uParamFlag);
			nRet = UGW_SUCCESS;	

		}
	}
	return nRet;
}

/*  =============================================================================
 *   Function Name 	: help_mergeObjList					*
 *   Description 	: Function merge two objlist , parameter value update.	*
 *  ============================================================================*/
uint32_t help_mergeObjList(INOUT ObjList *pxObjDst,IN ObjList *pxObjSrc)
{
 	ObjList *pxTmpObj;
	ParamList *pxParam;
	uint32_t nRet = UGW_SUCCESS;

	help_updateObjName(pxObjSrc, pxObjDst);

	FOR_EACH_OBJ(pxObjSrc,pxTmpObj)
	{
		FOR_EACH_PARAM(pxTmpObj,pxParam)
		{
			nRet = help_editNode(pxObjDst, pxTmpObj->sObjName, pxParam->sParamName, pxParam->sParamValue,
										pxParam->unParamId, pxParam->unParamFlag);
		}
		if (nRet != UGW_SUCCESS)
		{
			/* objects doesnt exists, add to the dst objlist */
			HELP_COPY_OBJ(pxObjDst, pxTmpObj, SOPT_OBJVALUE, COPY_SINGLE_OBJ);
			nRet = UGW_SUCCESS;
		}
		else if(list_empty(&(pxTmpObj->xParamList.xPlist)))
		{
			ObjList *pxObj;
			int32_t nFlag = NOT_SET;
			FOR_EACH_OBJ(pxObjDst, pxObj)
			{
				if(strcmp(pxObj->sObjName, pxTmpObj->sObjName) == 0)
				{
					nFlag=SET;
					pxObj->unSid = pxTmpObj->unSid;
					pxObj->unOid = pxTmpObj->unOid;
					pxObj->unObjFlag = pxTmpObj->unObjFlag;
					break;
				}
			}
			if(nFlag == NOT_SET)
			{
				/* objects doesnt exists, add to the dst objlist */
				HELP_COPY_OBJ(pxObjDst, pxTmpObj, SOPT_OBJVALUE, COPY_SINGLE_OBJ);
				nRet = UGW_SUCCESS;
			}
		}
		
	}
	return nRet;
}

/*  =============================================================================
 *   Function Name 	: help_getObjPtr					*
 *   Description 	: Function to get the objnode ptr based on the paramname*
 *			  and value.						*
 *  ============================================================================*/
ObjList* help_getObjPtr(ObjList *pxObj, const char *paramName, const char *paramValue)
{
	ObjList *pxTmpObj;
	ParamList *pxTmpParam;

	if(paramName == NULL || pxObj == NULL)
	{
		LOGF_LOG_ERROR("Requested object ptr can't find since paramName is NULL\n");
		return NULL;
	}

	FOR_EACH_OBJ(pxObj, pxTmpObj)
	{
		FOR_EACH_PARAM(pxTmpObj,pxTmpParam)
		{
			if((strcmp(paramName,pxTmpParam->sParamName) == 0 ) && (strnlen_s(paramName,MAX_LEN_PARAM_NAME) == strnlen_s(pxTmpParam->sParamName,MAX_LEN_PARAM_NAME)))
			{
				if(paramValue != NULL)
				{
			 		if( (strcmp(paramValue,pxTmpParam->sParamValue) == 0 )  && (strnlen_s(paramValue,MAX_LEN_PARAM_VALUE) == strnlen_s(pxTmpParam->sParamValue,MAX_LEN_PARAM_VALUE))) 
					{
						return pxTmpObj;
					}
				}
				else
				{	
					return pxTmpObj;
				}
			}
		}
	}
	return NULL;
}

/*  =============================================================================
 *   Function Name 	: help_DelObjPtr					*
 *   Description 	: Function to del the objnode ptr in head node objlist  *
 *  ============================================================================*/
int help_delObjPtr(ObjList *pxObjList, ObjList *pxDelObj)
{
	ObjList *pxTempObj;
	ParamList *pxParamList;
	
	while( !list_empty(&pxObjList->xOlist) ) 
	{
		pxTempObj = list_entry(pxObjList->xOlist.next,ObjList,xOlist);
		if (pxTempObj == pxDelObj)
		{
			while( !list_empty(&pxTempObj->xParamList.xPlist) ) 
			{
				pxParamList = list_entry(pxTempObj->xParamList.xPlist.next,ParamList,xPlist);
				list_del(&pxParamList->xPlist);
				free(pxParamList);
				pxParamList = NULL;
			}
			list_del(&pxTempObj->xOlist);
			free(pxTempObj);
			pxTempObj = NULL;
			return UGW_SUCCESS;
		}
		else
		{
			pxObjList = list_entry(pxObjList->xOlist.next,ObjList,xOlist);
		} 
	}
	return UGW_FAILURE;
}

/*  =============================================================================
 *   Function Name 	: help_moveObjList					*
 *   Description 	: Function to move paraticular objlist from one to 	*
 *			  another   						*
 *  ============================================================================*/
int help_moveObjList(ObjList *pxDstObj, ObjList *pxSrcObj, const char * pcObjName, uint32_t unFlag)
{
	ObjList *pxTmpObj=NULL;
	
	FOR_EACH_OBJ(pxSrcObj,pxTmpObj)
	{
		if((strcmp(GET_OBJ_NAME(pxTmpObj), pcObjName) == 0) && (strnlen_s(GET_OBJ_NAME(pxTmpObj),MAX_LEN_OBJNAME) == strnlen_s(pcObjName,MAX_LEN_OBJNAME)))
		{
			help_copyObjList(pxDstObj, unFlag, pxTmpObj);
			/* delete node from the src */
			help_delObjPtr(pxSrcObj,pxTmpObj);
			return UGW_SUCCESS;
		}
	 }
	return UGW_FAILURE;
}

/*  =============================================================================
 *   function : help_isEmptyObj							*
 *   Description :function to function find objlist is empty or not		*
 *  ============================================================================*/
OUT int help_isEmptyObj(IN ObjList *pxObj)
{
	ObjList *pxObjTmp;
	FOR_EACH_OBJ(pxObj,pxObjTmp)
	{
		return UGW_FAILURE;
	}
	return UGW_SUCCESS;
}

/*  =============================================================================
 *   function : help_isEmptyObjParam						*
 *   Description :function to find if a given objlist includes parameters or not*
 *  ============================================================================*/
OUT int help_isEmptyObjParam(IN ObjList *pxObj)
{
	ObjList *pxObjTmp;
	ParamList *pxParam;
	FOR_EACH_OBJ(pxObj,pxObjTmp)
	{
		FOR_EACH_PARAM(pxObjTmp, pxParam)
		{
			return UGW_FAILURE;
		}
	}
	return UGW_SUCCESS;
}

/*  =============================================================================
 *   Function Name 	: help_getValue						*
 *   Description 	: Function to get the value from Objlist		*
 *  ============================================================================*/
int help_getValue(IN ObjList *pxObj, IN uint32_t unOid, IN uint32_t unInstance, IN uint32_t unParamId, OUT char *pcVal)
{
	ObjList *pxTmpObj;
        ParamList *pxParam;
	uint32_t unCount=0;
	uint32_t nRet=UGW_FAILURE;

	if (unInstance == 0)
	{	
		FOR_EACH_OBJ(pxObj, pxTmpObj)
		{
			if(pxTmpObj->unOid == unOid) 
			{
				FOR_EACH_PARAM(pxTmpObj, pxParam)
				{
					if (pxParam->unParamId == unParamId) 
					{
						if (strncpy_s(pcVal, MAX_LEN_PARAM_VALUE, pxParam->sParamValue, MAX_LEN_PARAM_VALUE) != EOK) {
							LOGF_LOG_CRITICAL("strncpy_s failed\n");
						}
						nRet = UGW_SUCCESS;
						return nRet;
					}
				}
			}
		}
	}
	else
	{
		/* Instance based get, it depends on total number of entries of requested object */
		FOR_EACH_OBJ(pxObj, pxTmpObj)
		{
			if(pxTmpObj->unOid == unOid) 
			{
			 	unCount++;
				if (unCount == unInstance)
				{
					FOR_EACH_PARAM(pxTmpObj, pxParam)
					{
						if (pxParam->unParamId == unParamId) 
						{
							if (strncpy_s(pcVal, MAX_LEN_PARAM_VALUE, pxParam->sParamValue, MAX_LEN_PARAM_VALUE) != EOK) {
								LOGF_LOG_CRITICAL("strncpy_s failed\n");
							}		
							nRet = UGW_SUCCESS;
							return nRet;
						}
					}
				}
			}
		}
	}
	return nRet;
}

/*  =============================================================================
 *   Function Name 	: help_getUCIValue					*
 *   Description 	: Function to get the value from Objlist		*
 *  ============================================================================*/
int help_getUCIValue(IN UCIConfigData *pxUCIConfigData, IN char *psType, IN char *psName, IN char *psOptionName, OUT char **psValue, char *psValueType)
{
	int nRet = UGW_SUCCESS;
	int nLen = 0;
	int nOldLen = 0;
	bool bNextConfigSec = 0;
	bool bValFound = 0;
	char *psTemp = NULL;
	ParamList *pxParam = NULL;
	ParamList *pxParam1 = NULL;
	UCIConfigData *pxConfig = NULL;
	UCIConfigData *pxSectionHead = NULL;
	ArrayList *pxArrayValueListTmp = NULL;

	LOGF_LOG_DEBUG("Trying to find Config Parameter:%s (%s Type) of Config Type:%s in Config Section:%s ...\n", psOptionName, psValueType, psType, psName);

	if ((psType == NULL) || (psName  == NULL)) {
		LOGF_LOG_ERROR("Config Type and Config Section is not mandatory to get config value.\n");
		return UGW_FAILURE;
	}

	*psValue = NULL;

	FOR_EACH_CONFIG(pxUCIConfigData, pxConfig) {
		bNextConfigSec = 0;
		if (pxConfig->pxConfigList != NULL) {
			FOR_EACH_PARAM_ONLY(pxConfig->pxConfigList, pxParam) {
				LOGF_LOG_DEBUG("pxParam->sParamName:|%s| Section Name |%s| pxParam->sParamValue:|%s| \n", pxParam->sParamName, psName, pxParam->sParamValue);
				if (strcmp(pxParam->sParamName, ".name") == 0) {
					if (strcmp(pxParam->sParamValue, psName) == 0) { /* Section Name Matched */
						FOR_EACH_PARAM_ONLY(pxConfig->pxConfigList, pxParam1) {
							LOGF_LOG_DEBUG("pxParam->sParamName:|%s| Section Type: |%s| pxParam->sParamValue:|%s|\n", pxParam1->sParamName, psType, pxParam1->sParamValue);
							if (strcmp(pxParam1->sParamName, ".type") == 0) {
								if (strcmp(pxParam1->sParamValue, psType) == 0) { /* Section Type Matched */
									LOGF_LOG_DEBUG("Section found.\n");
									pxSectionHead =  pxConfig;
									goto sectionfound;
								} else {
									bNextConfigSec = 1;
								}
							}
							if (bNextConfigSec == 1)
								break;
						}
					} else {
						bNextConfigSec = 1;
					}
				}

				if (bNextConfigSec == 1)
					break;
			}
		}
	}

sectionfound:
	if (pxSectionHead != NULL) {
		if (strcmp(psValueType, "Option") == 0) { /* UCI Option Value */
			if (pxConfig->pxValueList != NULL) {
				FOR_EACH_PARAM_ONLY(pxConfig->pxValueList->pxParamValueList, pxParam) {
					LOGF_LOG_DEBUG("pxParam->sParamName:|%s| psOptionName: |%s| pxParam->sParamValue:|%s|\n", pxParam->sParamName, psOptionName, pxParam->sParamValue);
					if (strcmp(pxParam->sParamName, psOptionName) == 0) {
						LOGF_LOG_ERROR("Config Parameter:|%s| of Config Type:|%s| in Config Section:|%s| is found: |%s|\n", psOptionName, psType, psName, pxParam->sParamValue);
						*psValue = HELP_MALLOC(MAX_LEN_PARAM_VALUE * sizeof(char));
						if (*psValue != NULL) {
							if (strncpy_s(*psValue, (MAX_LEN_PARAM_VALUE * sizeof(char)), pxParam->sParamValue, (MAX_LEN_PARAM_VALUE * sizeof(char))) != EOK) {
								LOGF_LOG_CRITICAL("strncpy_s failed\n");
							}
							bValFound = 1;
						} else {
							LOGF_LOG_CRITICAL("MALLOC Failed.\n");
						}
						goto end;
					}
				}
			}
		} else { /* UCI List Value */
			if ((pxConfig->pxValueList != NULL) && (pxConfig->pxValueList->pxArrayValueList != NULL)) {
				FOR_EACH_ARRAYLIST(pxConfig->pxValueList->pxArrayValueList, pxArrayValueListTmp) {
					LOGF_LOG_ERROR("pxArrayValueListTmp->sArrayName:|%s| List Name:|%s|\n", pxArrayValueListTmp->sArrayName, psOptionName);
					if (strcmp(pxArrayValueListTmp->sArrayName, psOptionName) == 0) { /* List Found */
						FOR_EACH_PARAM_ONLY(pxArrayValueListTmp->pxArrayValues, pxParam) {
							LOGF_LOG_DEBUG("List Param:|%s| List Val:|%s| nLen:|%d|\n", psOptionName, pxParam->sParamValue, nLen);
							nOldLen = nLen;
							nLen = nLen + strnlen_s(pxParam->sParamValue, sizeof(pxParam->sParamValue));

							psTemp = realloc((*psValue), ((nLen+1) * sizeof(char)));
							if (psTemp != NULL) {
								if (nOldLen == 0) {
									*psValue = psTemp;
									if (strncpy_s((*psValue), ((strnlen_s(pxParam->sParamValue, sizeof(pxParam->sParamValue))+1) * sizeof(char)), pxParam->sParamValue, ((strnlen_s(pxParam->sParamValue, sizeof(pxParam->sParamValue))+1) * sizeof(char))) != EOK) {
										LOGF_LOG_CRITICAL("strncpy_s failed\n");
				}
								} else {
									if (sprintf_s((*psValue+nOldLen), ((strnlen_s(pxParam->sParamValue, sizeof(pxParam->sParamValue))+2) * sizeof(char)), ",%s", pxParam->sParamValue) <= 0) {
										LOGF_LOG_CRITICAL("sprintf_s failed\n");
									}
								}
							} else {
								LOGF_LOG_CRITICAL("realloc failure\n");
							}
						}
						bValFound = 1;
						goto end;
					}
				}
			}
		}
	}

end:
	if (bValFound == 0) {
		LOGF_LOG_ERROR("Config Parameter:%s of Config Type:%s in Config Section:%s not found !!!\n", psOptionName, psType, psName);
		nRet = UGW_FAILURE;
	} else {
		LOGF_LOG_DEBUG("Config Parameter:%s of Config Type:%s in Config Section:%s found: |%s| \n", psOptionName, psType, psName, *psValue);
		nRet = UGW_SUCCESS;
	}

	return nRet;
}

/*  =============================================================================
 *   Function Name 	: help_getValueNameBased				*
 *   Description 	: Function to get the value from Objlist thru 		*
 *			  object/parameter name match				*
 *  ============================================================================*/
int help_getValueNameBased(IN ObjList *pxObj, IN char *pcObjName, IN uint32_t unInstance, IN char *pcParamName, OUT char *pcVal)
{
	ObjList *pxTmpObj;
        ParamList *pxParam;
	uint32_t unCount=0;
	uint32_t nRet=UGW_FAILURE;

	/* object instance based or single instance object parameter value get */
	FOR_EACH_OBJ(pxObj, pxTmpObj)
	{
		if(strstr(GET_OBJ_NAME(pxTmpObj), pcObjName) != NULL)
		{
		 	unCount++;
			if (unInstance > 0 && unCount != unInstance)
			{
				continue;
			}	
			FOR_EACH_PARAM(pxTmpObj, pxParam)
			{
				if (strcmp(pxParam->sParamName,pcParamName) == 0)
				{
					if (strncpy_s(pcVal, MAX_LEN_PARAM_VALUE, pxParam->sParamValue, MAX_LEN_PARAM_VALUE) != EOK) {
						LOGF_LOG_CRITICAL("strncpy_s failed\n");
					}
					nRet = UGW_SUCCESS;
					return nRet;
				}
			}
		}
	}
	return nRet;
}
 
/*  =============================================================================
 *   Function Name 	: help_setValue						*
 *   Description 	: Function to set the value in given Objlist		*
 *  ============================================================================*/
int help_setValue(IN ObjList *pxObj, IN uint32_t unOid, IN uint32_t unInstance, IN uint32_t unParamId, OUT char *pcVal)
{
	ObjList *pxTmpObj;
        ParamList *pxParam;
	uint32_t unCount=0;
	uint32_t nRet=UGW_FAILURE;

	if (unInstance == 0)	
	{
		FOR_EACH_OBJ(pxObj, pxTmpObj)
		{
			if(pxTmpObj->unOid == unOid) 
			{
				FOR_EACH_PARAM(pxTmpObj, pxParam)
				{
					if (pxParam->unParamId == unParamId) 
					{
						if(pcVal != NULL)
						{
							if( strnlen_s(pcVal,MAX_LEN_PARAM_VALUE) < MAX_LEN_PARAM_VALUE)
							{
								if (strncpy_s(pxParam->sParamValue, MAX_LEN_PARAM_VALUE, pcVal, MAX_LEN_PARAM_VALUE) != EOK) {
									LOGF_LOG_CRITICAL("strncpy_s failed\n");
								}
								nRet=UGW_SUCCESS;
							}
							else
							{
								LOGF_LOG_CRITICAL("Buffer OverFlow \n");
								nRet = UGW_FAILURE;
							}
						}
						return nRet;
					}
				}
			}
		}
	}
	else
	{
		/* Instance based get, it depends on total number of entries of requested object */
		FOR_EACH_OBJ(pxObj, pxTmpObj)
		{
			if(pxTmpObj->unOid == unOid) 
			{
			 	unCount++;
				if (unCount == unInstance)
				{
					FOR_EACH_PARAM(pxTmpObj, pxParam)
					{
						if (pxParam->unParamId == unParamId) 
						{
							if(pcVal != NULL)
							{
								if(strnlen_s(pcVal,MAX_LEN_PARAM_VALUE) < MAX_LEN_PARAM_VALUE)
								{
									if (strncpy_s(pxParam->sParamValue, MAX_LEN_PARAM_VALUE, pcVal, MAX_LEN_PARAM_VALUE) != EOK) {
										LOGF_LOG_CRITICAL("strncpy_s failed\n");
									}
									nRet = UGW_SUCCESS;
								}
								else
								{	
									LOGF_LOG_CRITICAL("Buffer OverFlow \n");
									nRet = UGW_FAILURE;
								}
								return nRet;
							}
						}
					}
				}
			}
		}
	}

	return nRet;
}

/*  =============================================================================
 *   Function Name 	: help_setValueNameBased				*
 *   Description 	: Function to set the value in given Objlist thru	*
 *			  object/parameter name match				*
 *  ============================================================================*/
int help_setValueNameBased(IN ObjList *pxObj, IN char *pcObjName, IN uint32_t unInstance, IN char *pcParamName, OUT char *pcVal)
{
	ObjList *pxTmpObj;
        ParamList *pxParam;
	uint32_t unCount=0;
	uint32_t nRet=UGW_FAILURE;

	/* object instance based or single instance object parameter value get */
	FOR_EACH_OBJ(pxObj, pxTmpObj)
	{
		if(strstr(GET_OBJ_NAME(pxTmpObj), pcObjName) != NULL)
		{
		 	unCount++;
			if (unInstance > 0 && unCount != unInstance)
			{
				continue;
			}	
			FOR_EACH_PARAM(pxTmpObj, pxParam)
			{
				if (strcmp(pxParam->sParamName,pcParamName) == 0)
				{
					if(pcVal != NULL)
					{
						if( strnlen_s(pcVal,MAX_LEN_PARAM_VALUE) < MAX_LEN_PARAM_VALUE)
						{
							if (strncpy_s(pxParam->sParamValue, MAX_LEN_PARAM_VALUE, pcVal, MAX_LEN_PARAM_VALUE) != EOK) {
								LOGF_LOG_CRITICAL("strncpy_s failed\n");
							}
							nRet = UGW_SUCCESS;
						}
						else
						{
							LOGF_LOG_CRITICAL("Buffer OverFlow \n");
							nRet = UGW_FAILURE;
						}
					}
					return nRet;
				}
			}
		}
	}
	return nRet;
}

static int indexFromString(char *s) {
	unsigned int i;
	char * indexPtr;

	if (!s) {
		return -1;
	}

	i = strnlen_s(s,1024) - 1;
	while ((i > 0) && (s[i] != '_')) {
		i--;
	}

	if (i < 1) {
		return -1;
	}

	s[i] = '\0';
	indexPtr = &s[i+1];
	return atoi(indexPtr);
}
