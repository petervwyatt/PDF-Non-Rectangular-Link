# PDF 2.0 Extension - Non-Rectangular Links
Demonstration of the Non-Rectangular Links capabilities defined by [ISO/TS 24654:2022](https://www.iso.org/standard/79150.html) and prepared by ISO TC 171 SC 2 WG 7 (PDF for Engineering).

Prototype plug in for Adobe Acrobat DC. Tested with Microsoft Visual Studio 2019 in 32 bit. Use a CMD prompt with administrator priviledges to copy the NRLinkPId.api to the Adobe Acrobat DC plugin directory:  

```
copy NRLinkPId.api "C:\Program Files (x86)\Adobe\Acrobat DC\Acrobat\plug_ins"
```

When a PDF is open in Acrobat, two new menu options will appear in the Plug-Ins menu:
1. "**Create Non-Rectangular Link**" - click the mouse button to create straight-line segments of an arbitary path Link annotation outline (displayed in red). '+' to close. ESC to cancel. 
2. "**Paint Non-Rectangular Link paths**" - this renders each Link Annotation with a Path entry as visible. As a result, the PDF file is modified and shouldn't be saved.

See also [Introducing non-rectangular links to PDF](https://www.pdfa.org/introducing-non-rectangular-links-to-pdf/)
