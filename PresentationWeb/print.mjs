import puppeteer from 'puppeteer-core';

(async () => {
  const browser = await puppeteer.launch({
    executablePath: 'C:\\Program Files (x86)\\Microsoft\\Edge\\Application\\msedge.exe',
    headless: true
  });
  const page = await browser.newPage();
  await page.setViewport({ width: 1920, height: 1080 });
  await page.goto('http://localhost:5173/presentation/index.html', { waitUntil: 'networkidle0', timeout: 60000 });

  await page.addStyleTag({ content: `
    @media print {
      @page { size: 1920px 1080px; margin: 0; }
      body, html { margin: 0; padding: 0; height: auto; overflow: visible !important; }
      .deck { 
        position: static !important; 
        width: 100vw !important; 
        height: auto !important; 
        overflow: visible !important; 
        display: block !important; 
      }
      .slide { 
        position: relative !important; 
        visibility: visible !important; 
        opacity: 1 !important; 
        transform: none !important; 
        page-break-after: always !important; 
        break-after: page !important;
        width: 100vw !important; 
        height: 100vh !important; 
        z-index: 1 !important;
      }
      .menu-toggle, .slide-menu, .menu-scrim { display: none !important; }
    }
  `});

  // Give some time for embedded WebApp to load
  await new Promise(r => setTimeout(r, 5000));

  await page.pdf({
    path: 'AI_RE_Presentation.pdf',
    printBackground: true,
    landscape: true,
    preferCSSPageSize: true
  });

  await browser.close();
  console.log('PDF generation complete.');
})().catch(console.error);
