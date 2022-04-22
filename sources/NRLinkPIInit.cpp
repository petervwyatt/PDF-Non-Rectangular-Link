#ifndef MAC_PLATFORM
#include "stdafx.h"
#include "PIHeaders.h"
#endif

#include <algorithm>
#include <functional>
#include <vector>

static AVMenuItem menu_item_create = NULL;
static AVMenuItem menu_item_show = NULL;

ACCB1 ASBool ACCB2 IsFileOpen(void* clientData);
ACCB1 void ACCB2 CreateLink(void* clientData);
ACCB1 void ACCB2 ShowLink(void* clientData);

//*****************************************************************************
// Tool
ASBool gToolSelected;
AVToolRec gTool;
AVDevCoord gXPoints[1000];
AVDevCoord gYPoints[1000];
ASUns32 gNumPoints;
static AVPageViewDrawProc myDrawingProc;


static ACCB1 ASAtom ACCB2 ToolGetType(AVTool tool) {
  return ASAtomFromString("ISO_:NRLinksTool");
}

static ACCB1 void ACCB2 ToolActivate(AVTool tool, ASBool persistent) {
  gToolSelected = persistent;
  gNumPoints = 0;
}

static ACCB1 void ACCB2 ToolDeactivate(AVTool tool) {
  gToolSelected = false;
  gNumPoints = 0;
}

static ACCB1 ASBool ACCB2 ToolDoClick(AVTool tool, AVPageView pageView, AVDevCoord xHit, AVDevCoord yHit, AVFlagBits16 flags, AVTCount clickNo)
{
    PDColorValueRec color;
    color.space = PDDeviceRGB;
    color.value[0] = 0xffff; // RED line for Path
    color.value[1] = 0; 
    color.value[2] = 0;
    AVPageViewSetColor(pageView, &color);

    AVPageViewBeginOperation(pageView);
    gXPoints[gNumPoints] = xHit;
    gYPoints[gNumPoints] = yHit;
    gNumPoints++;
    AVPageViewDrawPolygonOutline(pageView, gXPoints, gYPoints, gNumPoints, false);
    AVPageViewEndOperation(pageView);
    return true;
}

static ASBool ToolDoKeyDown(AVTool tool, AVKeyCode key, AVFlagBits16 flags) {
    if (key == '+') {
        // Create a Link Annotation with a single Path entry 
        ASFixedRect rect = { 0,0,0,0 };
        AVPageView pageView = AVDocGetPageView(AVAppGetActiveDoc());
        PDPage page = AVPageViewGetPage(pageView);
        PDDoc pddoc = AVDocGetPDDoc(AVAppGetActiveDoc());
        CosDoc cosDoc = PDDocGetCosDoc(pddoc);
        PDAnnot annot = PDPageAddNewAnnot(page, -2, ASAtomFromString("Link"), &rect);

        CosObj cosAnnot = PDAnnotGetCosObj(annot);
        CosObj cosPath = CosNewArray(cosDoc, true, gNumPoints + 1);

        // use CropBox of Page as an excessive Rect for the Link annotation
        PDPageGetCropBox(page, &rect);

        // add at the end of the path the first point to have explicitly enclosed area
        for (ASUns32 i = 0; i < gNumPoints + 1; i++) {
            ASFixedPoint pagePoint;
            if (i == gNumPoints)
                AVPageViewDevicePointToPage(pageView, gXPoints[0], gYPoints[0], &pagePoint);
            else
                AVPageViewDevicePointToPage(pageView, gXPoints[i], gYPoints[i], &pagePoint);

            CosObj cosArray = CosNewArray(cosDoc, false, 2);
            CosArrayPut(cosArray, 0, CosNewFixed(cosDoc, false, pagePoint.h));
            CosArrayPut(cosArray, 1, CosNewFixed(cosDoc, false, pagePoint.v));

            CosArrayPut(cosPath, i, cosArray);
        }

        CosDictPut(cosAnnot, ASAtomFromString("Path"), cosPath);
        CosObj action = CosNewDict(cosDoc, false, 3);
        CosDictPut(action, ASAtomFromString("Type"), CosNewName(cosDoc, false, ASAtomFromString("Action")));
        CosDictPut(action, ASAtomFromString("S"), CosNewName(cosDoc, false, ASAtomFromString("URI")));
        CosDictPut(action, ASAtomFromString("URI"), CosNewString(cosDoc, true, "https://www.pdfa.org/iso-status/", 33));

        CosDictPut(cosAnnot, ASAtomFromString("A"), action);
        AVAlertNote("Non-rectangular Link annotation created (Path entry with a URI action).");

        // Determine whether Extensions/ISO_ developer extensions dictionary exists. If not, create it.
        CosObj docCat = CosDocGetRoot(cosDoc);
        CosObj extnsDict;
        ASBool isoExist = false;
        ASBool extnsExist = CosDictKnown(docCat, ASAtomFromString("Extensions"));
        if (extnsExist) {
            extnsDict = CosDictGet(docCat, ASAtomFromString("Extensions"));
            ASBool isoExist = CosDictKnown(extnsDict, ASAtomFromString("ISO_"));
            if (isoExist) {
                AVAlertNote("Document Catalog already contains an Extensions/ISO_ entry. Nothing was changed.");
            }
            else {
                AVAlertNote("Document Catalog already contains an Extensions dictionary, but no ISO_ entry. Adding.");
            }
        }
        else {
            extnsDict = CosNewDict(cosDoc, false, 2);
            AVAlertNote("Document Catalog does not have any Extensions dictionary - adding.");
        }

        if (!isoExist) {
            CosDictPut(extnsDict, ASAtomFromString("Type"), CosNewName(cosDoc, false, ASAtomFromString("Extensions")));
            CosObj devDict = CosNewDict(cosDoc, false, 2);
            CosDictPut(devDict, ASAtomFromString("Type"), CosNewName(cosDoc, false, ASAtomFromString("DeveloperExtensions")));
            CosDictPut(devDict, ASAtomFromString("BaseVersion"), CosNewName(cosDoc, false, ASAtomFromString("2.0")));
            CosDictPut(devDict, ASAtomFromString("ExtensionLevel"), CosNewInteger(cosDoc, false, 24654));
            CosDictPut(devDict, ASAtomFromString("ExtensionRevision"), CosNewString(cosDoc, false, "2022", 4));
            CosDictPut(devDict, ASAtomFromString("URL"), CosNewString(cosDoc, false, "https://www.iso.org/standard/79150.html", 40));
            CosDictPut(extnsDict, ASAtomFromString("ISO_"), devDict);
            CosDictPut(docCat, ASAtomFromString("Extensions"), extnsDict);
        }

        CosDocSetDirty(cosDoc, true);
        AVAppSetActiveTool(AVAppGetLastActiveTool(), true);
        return true;
    }
  
    if (key == 27) { // ESC
        AVAppSetActiveTool(AVAppGetLastActiveTool(), true);
        return true;
    }

    return false;
}


//*****************************************************************************
ACCB1 void ACCB2 myAVAppRegisterForPageViewDrawingProc(AVPageView pageView, AVDevRect* updateRect, void* data)
{
    PDPage page = AVPageViewGetPage(pageView);
    CosDoc cosDoc = PDDocGetCosDoc(PDPageGetDoc(page));
    AVPageViewBeginOperation(pageView);

    ASBool dirty = false;
  
    for (int i = 0; i < PDPageGetNumAnnots(page); i++) {
        PDAnnot annot = PDPageGetAnnot(page, i);
        if (PDAnnotGetSubtype(annot) == ASAtomFromString("Link")) {
            CosObj cosAnnot = PDAnnotGetCosObj(annot);
            CosObj cosPath = CosDictGet(cosAnnot, ASAtomFromString("Path"));

            ASFixedRect fixedBBox;
            fixedBBox.left = ASInt16ToFixed(10000);
            fixedBBox.right = ASInt16ToFixed(0);
            fixedBBox.bottom = ASInt16ToFixed(10000);
            fixedBBox.top = ASInt16ToFixed(0);

            // Constrain X and Y 
            auto setminmaxX = [&](ASFixed x) {
                if (x < fixedBBox.left) fixedBBox.left = x;
                if (x > fixedBBox.right) fixedBBox.right = x;
                return ASFixedToFloat(x);
            };
            auto setminmaxY = [&](ASFixed y) {
                if (y < fixedBBox.bottom) fixedBBox.bottom = y;
                if (y > fixedBBox.top) fixedBBox.top = y;
                return ASFixedToFloat(y);
            };

            char buf[2048], buf2[2048];
            memset(&buf, 0, sizeof(buf));
            strcat(buf, "q 2 w ");

            // Set different colors for different Paths (lowest 3 bits)
            switch (i & 0x0007) {
            case 0: strcat(buf, "0 1 0 RG "); break;
            case 1: strcat(buf, "0 0 1 RG "); break;
            case 2: strcat(buf, "1 0 0 RG "); break;
            case 3: strcat(buf, "1 1 0 RG "); break;
            case 4: strcat(buf, "1 0 1 RG "); break;
            case 5: strcat(buf, "0 1 1 RG "); break;
            case 6: strcat(buf, "1 1 1 RG "); break;
            default: strcat(buf, "0.9 0.3 0.6 RG "); break;
            }

            if (CosObjGetType(cosPath) == CosArray) {
                // have Link annotation with Path entry which is an Array
                // Going to dynamically generate content stream from the path segments 
                for (int pathIndex = 0; pathIndex < CosArrayLength(cosPath); pathIndex++) {
                    CosObj cosArray = CosArrayGet(cosPath, pathIndex);
                    if (CosObjGetType(cosArray) != CosArray)
                        continue;
                    int numNumbers = CosArrayLength(cosArray);
                    sprintf(buf2, " %f %f",
                        setminmaxX(CosFixedValue(CosArrayGet(cosArray, 0))),
                        setminmaxY(CosFixedValue(CosArrayGet(cosArray, 1)))
                    );
                    strcat(buf, buf2);
                    if (numNumbers == 6) {
                        sprintf(buf2, " %f %f %f %f",
                            setminmaxX(CosFixedValue(CosArrayGet(cosArray, 2))),
                            setminmaxY(CosFixedValue(CosArrayGet(cosArray, 3))),
                            setminmaxX(CosFixedValue(CosArrayGet(cosArray, 4))),
                            setminmaxY(CosFixedValue(CosArrayGet(cosArray, 5)))
                        );
                        strcat(buf, buf2);
                    }
                    // 2 entries means l=line
                    // 6 entries means c=bezier
                    // first element has 2 entries and means m=moveto
                    if (numNumbers == 2) {
                        if (pathIndex == 0)
                            strcat(buf, " m ");
                        else
                            strcat(buf, " l ");
                    }
                    else 
                        strcat(buf, " c ");
                } // for
            } // if
            strcat(buf," h S Q"); // close path, stroke and pop graphics state
            // AVAlertNote(buf);     // show the stream content

            ASUns32 bufferLen = (ASUns32)strlen(buf);

            // generating form XObject to render it with AVPageViewDrawCosObj()
            ASStm stm = ASMemStmRdOpen(buf, bufferLen);
            CosObj attributesDict = CosNewDict(cosDoc, false, 5);
            CosDictPutKeyString(attributesDict, "Length", CosNewInteger(cosDoc, false, bufferLen));
            CosObj formObj = CosNewStream(cosDoc, true, stm, 0, true, attributesDict, CosNewNull(), bufferLen);
            CosObj stmDictObj = CosStreamDict(formObj);
            CosDictPutKeyString(stmDictObj, "Type", CosNewNameFromString(cosDoc, false, "XObject"));
            CosDictPutKeyString(stmDictObj, "Subtype", CosNewNameFromString(cosDoc, false, "Form"));

            CosObj bboxObj = CosNewArray(cosDoc, false, 4L);
            CosArrayInsert(bboxObj, 0, CosNewFixed(cosDoc, false, fixedBBox.left));
            CosArrayInsert(bboxObj, 1, CosNewFixed(cosDoc, false, fixedBBox.bottom));
            CosArrayInsert(bboxObj, 2, CosNewFixed(cosDoc, false, fixedBBox.right));
            CosArrayInsert(bboxObj, 3, CosNewFixed(cosDoc, false, fixedBBox.top));
            CosDictPutKeyString(stmDictObj, "BBox", bboxObj);
            AVDevRect rect;
            // AVPageViewGetAnnotRect(pageView, annot, &rect);
            AVPageViewRectToDevice(pageView, &fixedBBox, &rect);
            AVPageViewDrawCosObj(pageView, formObj, &rect);
            dirty = true;
            CosObjDestroy(formObj);
        }
    }
    AVPageViewEndOperation(pageView);
    if (dirty)
        AVPageViewDrawNow(pageView); // redraw screen
}

//*****************************************************************************
ACCB1 void ACCB2 ShowLink(void* clientData) {
    AVAlertNote("Paint all Link annotations Path outlines.");
    myDrawingProc = ASCallbackCreateProto(AVPageViewDrawProc, myAVAppRegisterForPageViewDrawingProc);
    AVAppRegisterForPageViewDrawing(myDrawingProc, NULL);
    AVPageViewDrawNow(AVDocGetPageView(AVAppGetActiveDoc()));
}

//*****************************************************************************
ACCB1 void ACCB2 CreateLink(void* clientData) {
    AVAlertNote("Define an arbitrary Link area by clicking on the page.\nPress '+' to close the path and create a non-rectangular Link annotation.\nESC to cancel.");
    AVAppSetActiveTool(&gTool, true);
}

//*****************************************************************************
ACCB1 ASBool ACCB2 IsFileOpen(void* clientData) {
    return (AVAppGetActiveDoc() != NULL);
}

//*****************************************************************************
ACCB1 ASBool ACCB2 PluginExportHFTs() {
    return true;
}

ACCB1 ASBool ACCB2 PluginImportReplaceAndRegister() {
    return true;
}

static void SetUpTool() {
    memset(&gTool, 0, sizeof(AVToolRec));
    gTool.size = sizeof(AVToolRec);

    gTool.Activate = ASCallbackCreateProto(ActivateProcType, &ToolActivate);
    gTool.Deactivate = ASCallbackCreateProto(DeactivateProcType, &ToolDeactivate);
    gTool.GetType = ASCallbackCreateProto(GetTypeProcType, &ToolGetType);
    gTool.DoKeyDown = ASCallbackCreateProto(DoKeyDownProcType, &ToolDoKeyDown);
    gTool.DoClick = ASCallbackCreateProto(DoClickProcType, &ToolDoClick);
    AVAppRegisterTool(&gTool);
}

ACCB1 ASBool ACCB2 PluginInit() {
    AVMenubar menubar = AVAppGetMenubar();
    if (!menubar)
        return false;

    auto parent_menu = AVMenubarAcquireMenuByName(menubar, "Extensions");

    DURING

    // Create our menu items
    menu_item_create = AVMenuItemNew("Create Non Rectangular Link", "ISO_:NRLink:CreateLink", NULL, true, NO_SHORTCUT, 0, NULL, gExtensionID);
    AVMenuItemSetExecuteProc(menu_item_create, ASCallbackCreateProto(AVExecuteProc, CreateLink), NULL);
    AVMenuItemSetComputeEnabledProc(menu_item_create, ASCallbackCreateProto(AVComputeEnabledProc, IsFileOpen), (void*)pdPermEdit);
    AVMenuAddMenuItem(parent_menu, menu_item_create, APPEND_MENUITEM);

    menu_item_show = AVMenuItemNew("Paint Non Rectangular Link paths", "ISO_:NRLink:ShowLink", NULL, true, NO_SHORTCUT, 0, NULL, gExtensionID);
    AVMenuItemSetExecuteProc(menu_item_show, ASCallbackCreateProto(AVExecuteProc, ShowLink), NULL);
    AVMenuItemSetComputeEnabledProc(menu_item_show, ASCallbackCreateProto(AVComputeEnabledProc, IsFileOpen), (void*)pdPermEdit);
    AVMenuAddMenuItem(parent_menu, menu_item_show, APPEND_MENUITEM);
    SetUpTool();

    HANDLER
    END_HANDLER
    return true;
}

ACCB1 ASBool ACCB2 PluginUnload() {
    if (menu_item_create)
        AVMenuItemRemove(menu_item_create);
    if (menu_item_show)
        AVMenuItemRemove(menu_item_show);
    return true;
}

ASAtom GetExtensionName() {
    return ASAtomFromString("ISO_:NonRectangularLink");
}


/**
  Function that provides the initial interface between your plug-in and the application.
  This function provides the callback functions to the application that allow it to
  register the plug-in with the application environment.

  Required Plug-in handshaking routine: <b>Do not change it's name!</b>

  @param handshakeVersion the version this plug-in works with. There are two versions possible, the plug-in version
  and the application version. The application calls the main entry point for this plug-in with its version.
  The main entry point will call this function with the version that is earliest.
  @param handshakeData OUT the data structure used to provide the primary entry points for the plug-in. These
  entry points are used in registering the plug-in with the application and allowing the plug-in to register for
  other plug-in services and offer its own.
  @return true to indicate success, false otherwise (the plug-in will not load).
*/
ACCB1 ASBool ACCB2 PIHandshake(Uns32 handshakeVersion, void* handshakeData) {
  if (handshakeVersion == HANDSHAKE_V0200) {
    /* Cast handshakeData to the appropriate type */
    PIHandshakeData_V0200* hsData = (PIHandshakeData_V0200*)handshakeData;

    /* Set the name we want to go by */
    hsData->extensionName = GetExtensionName();

    /* If you export your own HFT, do so in here */
    hsData->exportHFTsCallback = (void*)ASCallbackCreateProto(PIExportHFTsProcType, &PluginExportHFTs);

    /*
    ** If you import plug-in HFTs, replace functionality, and/or want to register for notifications before
    ** the user has a chance to do anything, do so in here.
    */
    hsData->importReplaceAndRegisterCallback = (void*)ASCallbackCreateProto(PIImportReplaceAndRegisterProcType,
      &PluginImportReplaceAndRegister);

    /* Perform your plug-in's initialization in here */
    hsData->initCallback = (void*)ASCallbackCreateProto(PIInitProcType, &PluginInit);

    /* Perform any memory freeing or state saving on "quit" in here */
    hsData->unloadCallback = (void*)ASCallbackCreateProto(PIUnloadProcType, &PluginUnload);

    /* All done */
    return true;

  } /* Each time the handshake version changes, add a new "else if" branch */

  /*
  ** If we reach here, then we were passed a handshake version number we don't know about.
  ** This shouldn't ever happen since our main() routine chose the version number.
  */
  return false;
}
