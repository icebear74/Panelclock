#include <Adafruit_GFX.h>

// --- Farbpalette (RGB565 Format) ---
// Du kannst diese Werte anpassen, je nach Geschmack und Display-Gamma
#define COL_BG_WALL     0xCE79  // Helles Steingrau (Wand)
#define COL_FLOOR       0x9CD3  // Etwas dunklerer Boden
#define COL_MANTEL      0x8C71  // Mittelbraunes Holz (Sims)
#define COL_STONE       0xBDF7  // Heller Stein (Kaminumrandung)
#define COL_VOID        0x10A2  // Fast Schwarz (Kamin-Inneres)
#define COL_METAL       0x2124  // Dunkles Metall (Grau-Schwarz)
#define COL_LOG_BARK    0x52AA  // Dunkles Holz
#define COL_LOG_CUT     0xE5D6  // Helles Schnitt-Holz
#define COL_MIRROR_FR   0x8C51  // Holzrahmen Spiegel
#define COL_MIRROR_GL   0xDF1F  // Helles Blau-Grau (Glas)

/**
 * Zeichnet den Kamin auf einem übergebenen Display-Objekt.
 * @param display Zeiger auf das Adafruit_GFX kompatible Display-Objekt
 */
void drawProceduralFireplace(Adafruit_GFX *display) {
    
    // 1. Grundierung (Wand und Boden)
    display->fillScreen(COL_BG_WALL);
    
    // Bodenplatte (untere 8 Pixel)
    display->fillRect(0, 56, 192, 8, COL_FLOOR);
    // Fugen im Boden (für Fliesen-Look)
    for(int x = 20; x < 192; x+=40) {
        display->drawLine(x, 56, x-10, 64, 0x73AE); // Leichte Schräge für Perspektive
    }

    // --- DER KAMIN (Mitte) ---
    int fp_x = 66;  // Start X Kamin
    int fp_y = 22;  // Start Y Kamin (unter dem Spiegel)
    int fp_w = 60;  // Breite Kamin
    int fp_h = 34;  // Höhe bis zum Boden

    // Kaminumrandung (Stein)
    display->fillRect(fp_x, fp_y, fp_w, fp_h, COL_STONE);
    
    // Das schwarze "Loch" (Feuerraum)
    // Wir lassen einen Rand für die Steine links, rechts und oben
    display->fillRect(fp_x + 10, fp_y + 10, fp_w - 20, fp_h - 10, COL_VOID);

    // Kamin-Sims (Das Holzbrett oben drauf)
    // Etwas breiter als der Steinbau
    display->fillRect(fp_x - 4, fp_y - 3, fp_w + 8, 3, COL_MANTEL);

    // Feuerrost (Andeutung im schwarzen Loch)
    int grate_y = fp_y + fp_h - 4;
    display->drawLine(fp_x + 15, grate_y, fp_x + fp_w - 15, grate_y, COL_METAL);
    display->drawLine(fp_x + 15, grate_y-2, fp_x + 15, grate_y, COL_METAL); // Beine
    display->drawLine(fp_x + fp_w - 15, grate_y-2, fp_x + fp_w - 15, grate_y, COL_METAL);

    // --- DER SPIEGEL (Oben Mitte) ---
    int mir_x = 96; // Genau Mitte von 192
    int mir_y = 10; // Oben an der Wand
    int mir_r = 7;  // Radius

    // Rahmen
    display->fillCircle(mir_x, mir_y, mir_r, COL_MIRROR_FR);
    // Glas
    display->fillCircle(mir_x, mir_y, mir_r - 2, COL_MIRROR_GL);
    // Reflexions-Strich im Spiegel (Detail)
    display->drawLine(mir_x - 2, mir_y - 2, mir_x + 1, mir_y + 1, 0xFFFF);


    // --- HOLZLAGER (Links) ---
    int log_x = 20;
    int log_y = 28;
    int log_w = 24;
    int log_h = 28;

    // Metallrahmen
    display->drawRect(log_x, log_y, log_w, log_h, COL_METAL);
    display->drawRect(log_x+1, log_y, log_w-2, log_h, COL_METAL); // Doppelt für Dicke

    // Holzscheite stapeln (Kreise)
    // Wir loopen durch den Rahmenbereich
    for(int ly = log_y + 24; ly > log_y + 2; ly -= 5) { // Von unten nach oben
        for(int lx = log_x + 3; lx < log_x + log_w - 3; lx += 5) {
            // Holzscheit Außen (Rinde)
            display->fillCircle(lx, ly, 2, COL_LOG_BARK);
            // Holzscheit Innen (Schnitt)
            display->drawPixel(lx, ly, COL_LOG_CUT);
        }
    }


    // --- KAMINWERKZEUG (Rechts) ---
    int tool_x = 160;
    int tool_y = 26;
    
    // Ständer (Mittelstange)
    display->drawLine(tool_x, tool_y, tool_x, tool_y + 28, COL_METAL);
    // Fuß des Ständers
    display->drawLine(tool_x - 4, tool_y + 28, tool_x + 4, tool_y + 28, COL_METAL);
    // Aufhängung oben (Kreuz)
    display->drawLine(tool_x - 4, tool_y + 2, tool_x + 4, tool_y + 2, COL_METAL);

    // Werkzeug 1: Schaufel (hängt links)
    display->drawLine(tool_x - 3, tool_y + 2, tool_x - 3, tool_y + 20, COL_METAL);
    display->fillRect(tool_x - 4, tool_y + 20, 3, 4, COL_METAL); // Blatt

    // Werkzeug 2: Besen (hängt rechts)
    display->drawLine(tool_x + 3, tool_y + 2, tool_x + 3, tool_y + 20, COL_METAL);
    // Borsten (kleines Dreieck oder Rechteck)
    display->drawLine(tool_x + 2, tool_y + 20, tool_x + 4, tool_y + 20, COL_METAL); 
    display->drawLine(tool_x + 2, tool_y + 20, tool_x + 1, tool_y + 24, COL_METAL); // Buschig
    display->drawLine(tool_x + 4, tool_y + 20, tool_x + 5, tool_y + 24, COL_METAL);

    // Fertig!
}