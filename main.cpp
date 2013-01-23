/**
 * apkenvui
 * Copyright (c) 2013, crow_riot <crow@riot.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
 * IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 **/

#include <cstdlib>
#include <SDL.h>
#include <SDL/SDL_image.h>
#include <SDL/SDL_rotozoom.h>
#include <SDL/SDL_ttf.h>
#include <vector>
#include <string>
#include <dirent.h>
#include <unistd.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <strings.h>
#include <iostream>


#define SCREENWIDTH      800
#define SCREENHEIGHT     480
#define SCREENBITS       16
#define BACKGROUNDIMAGE  "res/background.png"
#define FONTFACE         "res/DroidSans.ttf"
#define FONTHEIGHTBIG    16
#define FONTHEIGHTSMALL  12
#define ICONOFFSET       8
#define TEXTOFFSET       4
#define CLIPBORDER       4
#define ICONMAXWIDTH     72
#define ICONMAXHEIGHT    72
#define WIDGETWIDTH      100
#define WIDGETHEIGHT     100
#define FONTCOLOR        {200,200,200,0}

#ifdef PANDORA
#define SDL_VIDEOMODE (SDL_SWSURFACE|SDL_FULLSCREEN|SDL_DOUBLEBUF)
#else
#define SDL_VIDEOMODE (SDL_HWSURFACE|SDL_DOUBLEBUF)
#endif

#define APKFOLDER "./apks"
#define ICONCACHEFOLDER "./iconcache"
#define RUNAPK "./runapk.sh"


extern "C"
{

#include "../apkenv/apklib/apklib.h"

void recursive_mkdir(const char *directory)
{
    char *tmp = strdup(directory);
    struct stat st;
    int len = strlen(directory);
    int i;

    /* Dirty? Works for me... */
    for (i=1; i<len; i++) {
        if (tmp[i] == '/') {
            tmp[i] = '\0';
            if (stat(tmp, &st) != 0) {
                mkdir(tmp, 0700);
            }
            tmp[i] = '/';
        }
    }

    free(tmp);
}

}

using namespace std;


string my_realpath(const char* dir0)
{
    if (dir0[0]=='~') {
        char* home = getenv("HOME");
        if (home) {
            string path(home);
            path += dir0+1;
            return path;
        }
    } else {
        char tmp[PATH_MAX];
        realpath(dir0,tmp);
        return string(tmp);
    }
    return "";
}

bool file_exists( const string& file )
{
    struct stat st;
    return stat(file.c_str(),&st)>=0;
}

/* -------- */

class Widget
{
public:
    Widget() :
        m_icon(NULL),
        m_row(-1),
        m_col(-1),
        m_text(NULL)
    {
        memset(&m_icon_rect,0,sizeof(m_icon_rect));
        memset(&m_text_rect,0,sizeof(m_text_rect));
        memset(&m_full_rect,0,sizeof(m_full_rect));
    }

    /** selection **/

    /// pick the widget, returns true if mx and my are within the widget bounds
    bool pick( int mx, int my )
    {
        return mx>=m_icon_rect.x && mx<=m_icon_rect.x+m_icon_rect.w
             && my>=m_icon_rect.y && my<=m_icon_rect.y+m_icon_rect.h;
    }

    /** sdl surface **/

    bool load_icon_surface(const string& iconpath, int maxwidth, int maxheight)
    {
        if (file_exists(iconpath) && m_icon==NULL) {
            m_icon = IMG_Load(iconpath.c_str());

            if (m_icon!=NULL && (m_icon->w>maxwidth || m_icon->h>maxheight))
            {
                resize_icon(maxwidth,maxheight);
            }
        }
        return m_icon!=NULL;
    }

    void set_text(const string& text, TTF_Font* font)
    {
        SDL_Color clr = FONTCOLOR;
        m_text = TTF_RenderText_Blended(font,text.c_str(),clr);
    }

    SDL_Surface* get_icon_surface() const
    {
        return m_icon;
    }

    /** icon rect **/

    void set_rect( const SDL_Rect& rect )
    {
        m_full_rect = rect;
        m_icon_rect = rect;
        m_text_rect = rect;
    }

    void align_rect()
    {
        if(m_icon!=NULL) {
            m_icon_rect.x = m_icon_rect.x + (m_icon_rect.w-m_icon->w)/2;
            m_icon_rect.y += ICONOFFSET;
        }
        if (m_text!=NULL) {
            m_text_rect.x = m_text_rect.x + (m_text_rect.w-m_text->w)/2;
            m_text_rect.y = m_text_rect.y + m_text_rect.h - m_text->h - TEXTOFFSET;
        }
    }

    void blit_to(SDL_Surface* target)
    {
        SDL_Rect cliprect = m_full_rect;
        cliprect.x += CLIPBORDER;
        cliprect.w -= CLIPBORDER*2;

        SDL_SetClipRect(target,&cliprect);
        if (m_icon) {
            SDL_BlitSurface(m_icon,NULL,target,&m_icon_rect);
        }
        if (m_text) {
            SDL_BlitSurface(m_text,NULL,target,&m_text_rect);
        }
        SDL_SetClipRect(target,NULL);
    }


    /** grid position */

    void set_row_column( int row, int col )
    {
        m_row = row;
        m_col = col;
    }
    int get_row() const
    {
        return m_row;
    }
    int get_col() const
    {
        return m_col;
    }

protected:
    void resize_icon(int maxwidth, int maxheight)
    {
        SDL_PixelFormat *format = m_icon->format;

        float aspect = float(maxwidth)/float(maxheight);
        SDL_Surface *scaled =
            zoomSurface(m_icon,float(maxwidth)/float(m_icon->w), float(maxheight)/float(m_icon->h)*aspect,SMOOTHING_ON);
        SDL_FreeSurface(m_icon);
        m_icon = scaled;
    }

private:
    SDL_Rect m_full_rect;
    SDL_Rect m_icon_rect;
    SDL_Surface *m_icon;
    SDL_Rect m_text_rect;
    SDL_Surface *m_text;
    int m_row;
    int m_col;
};


/* -------- */

class AndroidApkEx : public Widget
{
public:
    static AndroidApkEx* S_CurrentApk;
    static void extract_icon_callback(const char* filename, char* buf, size_t size)
    {
        if (S_CurrentApk->icon_exists()==false)
        {
            if (strstr(filename,"icon")!=0)
            {
                const char* ext = strrchr(filename,'.');
                if (ext && strcmp(ext,".png")==0)
                {
                    FILE *fp = fopen(S_CurrentApk->m_apk_iconpath.c_str(),"wb");
                    if (fp)
                    {
                        fwrite(buf,size,1,fp);
                        fclose(fp);
                    }
                }
            }
        }
    }


    AndroidApkEx( const string& folder, const string& name )
    {
        m_apk_basename = name;
        m_apk_filepath = folder+"/"+name;
        m_apk_iconpath = my_realpath(ICONCACHEFOLDER) + "/" + name + ".png";
        m_apk = apk_open(m_apk_filepath.c_str());
    }

    ~AndroidApkEx()
    {
        if (m_apk)
            apk_close(m_apk);
    }

    string get_apk_filename() const
    {
        return m_apk_filepath;
    }

    string get_apk_basename() const
    {
        return m_apk_basename;
    }

    AndroidApk* get_apk() const
    {
        return m_apk;
    }


    /** apk icon **/

    void extract_icon()
    {
        const char* iconprefixes[] = {
            "res/drawable-hdpi/icon",
            "res/drawable/icon",
            "res/drawable-hdpi",
            "res/drawable",
            0
        };

        S_CurrentApk = this;
        int i=0;
        while(!icon_exists() && iconprefixes[i]) {
            apk_for_each_file(m_apk,iconprefixes[i],extract_icon_callback);
            i++;
        }
        S_CurrentApk = NULL;
    }

    bool load_icon(int maxw, int maxh)
    {
        return load_icon_surface(m_apk_iconpath,maxw,maxh);
    }

    bool icon_exists()
    {
        return file_exists(m_apk_iconpath);
    }

private:
    AndroidApk* m_apk;
    string m_apk_filepath;
    string m_apk_iconpath;
    string m_apk_basename;
};
AndroidApkEx*  AndroidApkEx::S_CurrentApk = 0;

///
int list_apks( const char* dir0, vector<AndroidApkEx*>* apks )
{
    string directory = my_realpath(dir0);

    DIR* dir = opendir(directory.c_str());
    if (!dir) {
        cerr << "Failed to open directory: " << directory << endl;
        return 0;
    }

    struct dirent* entry = 0;
    while ((entry=readdir(dir))!=0)
    {
        const char* ext = strrchr(entry->d_name,'.');
        if (ext!=NULL && strcmp(ext,".apk")==0)
        {
            apks->push_back(new AndroidApkEx(directory,entry->d_name));
        }
    }
    closedir(dir);

    return apks->size();
}

void init_widgets(const vector<AndroidApkEx*>& apks, TTF_Font* font)
{
    for (int i=0,n=apks.size(); i<n; i++ ) {
        apks[i]->extract_icon();
        if (!apks[i]->load_icon(ICONMAXWIDTH,ICONMAXHEIGHT)) {
            cerr << "Failed to load Icon for " << apks[i]->get_apk_filename() << endl;
        } else {
            cerr << "Icon loaded for " << apks[i]->get_apk_filename() << endl;
        }
        apks[i]->set_text(apks[i]->get_apk_basename(),font);
    }
}

void align_widgets(SDL_Surface *target, const vector<AndroidApkEx*>& apks)
{
    SDL_Rect rect = {0,0,WIDGETWIDTH,WIDGETHEIGHT};

    int row = 0, col = 0;
    for (int i=0,n=apks.size(); i<n; i++ ) {

        apks[i]->set_rect(rect);
        apks[i]->set_row_column(row,col);
        apks[i]->align_rect();

        col ++;
        rect.x += WIDGETWIDTH;
        if (rect.x+WIDGETWIDTH>target->w)
        {
            col = 0; row ++;
            rect.x = 0;
            rect.y += WIDGETHEIGHT;
        }
    }
}

void draw_widgets(SDL_Surface *target, const vector<AndroidApkEx*>& apks)
{
    for (int i=0,n=apks.size(); i<n; i++ ) {
        apks[i]->blit_to(target);
    }
}

AndroidApkEx* select_apk(int mx, int my, const vector<AndroidApkEx*>& apks )
{
    for (int i=0,n=apks.size(); i<n; i++)
    {
        if (apks[i]->pick(mx,my))
            return apks[i];
    }

    return NULL;
}

/** text surface **/

class TextSurface
{
public:
    TextSurface( const char* text, TTF_Font* font, const SDL_Color& color )
    {
        m_lineheight = TTF_FontHeight(font);

        vector<string> lines;
        string currentline;

        const char* textptr = text;
        const char* textend = text + strlen(text);

        while (textptr<=textend)
        {
            if (*textptr=='\n') {
                //lines.push_back(currentline);
                //currentline.clear();
                if (currentline.size()) {
                    m_lines.push_back(TTF_RenderText_Blended(font,currentline.c_str(),color));
                } else {
                    m_lines.push_back(NULL); //empty line
                }
                currentline.clear();
            }
            else {
                currentline += *textptr;
            }
            *textptr ++;
        }

        if (currentline.size()) {
            m_lines.push_back(TTF_RenderText_Blended(font,currentline.c_str(),color));
        }
    }

    void blit_to(SDL_Surface* target)
    {
        int y = (target->h>>1) - m_lineheight * (m_lines.size()-1);
        for (int i=0,n=m_lines.size(); i<n; i++){
            y = draw_line_centered(i,target,y);
        }
    }


protected:
    int draw_line_centered(int line, SDL_Surface *target, int y)
    {
        if(m_lines[line])
        {
            SDL_Surface *textsurface = m_lines[line];
            SDL_Rect targetrect = {
                target->w-textsurface->w>>1,
                y,
                textsurface->w,
                textsurface->h
            };
            SDL_BlitSurface(textsurface,NULL,target,&targetrect);
            return y + textsurface->h;
        }
        else
        {
            return y + m_lineheight;
        }
    }

private:
    vector<SDL_Surface*> m_lines;
    int m_lineheight;
};



/** main */

int main ( int argc, char** argv )
{
    if (TTF_Init()<0)
    {
        cerr << "Unable to init TTF: " << TTF_GetError()  << endl;
        return 1;
    }
    // initialize SDL video
    if (SDL_Init(SDL_INIT_VIDEO|SDL_INIT_JOYSTICK) < 0)
    {
        cerr << "Unable to init SDL: " << SDL_GetError()  << endl;
        return 1;
    }

    if ((SDL_VIDEOMODE&SDL_FULLSCREEN)==0) {
        SDL_putenv("SDL_VIDEO_CENTERED=center"); //Center the game Window
    }

#ifdef PANDORA
    SDL_ShowCursor(0);
#endif

    mkdir( my_realpath(ICONCACHEFOLDER).c_str(), 0700 );
    mkdir( my_realpath(APKFOLDER).c_str(), 0700 );

    // create a new window
    SDL_Surface* screen = SDL_SetVideoMode(SCREENWIDTH, SCREENHEIGHT, SCREENBITS, SDL_VIDEOMODE);
    if ( !screen )
    {
        printf("Unable to set %dx%d video: %s\n", SCREENWIDTH,SCREENHEIGHT,SDL_GetError());
        return 1;
    }


// load background image
    SDL_Surface* background = IMG_Load(BACKGROUNDIMAGE);
// fooonts
    TTF_Font* fontbig = TTF_OpenFont(FONTFACE,FONTHEIGHTBIG);
    TTF_Font* fontsmall = TTF_OpenFont(FONTFACE,FONTHEIGHTSMALL);
    SDL_Color fontcolor = FONTCOLOR;

// prepare info text rendering
    char errotext[1024]; sprintf(errotext,"No APKs have been found.\n\nPlease put them into the following folder:\n%s",my_realpath(APKFOLDER).c_str());
    TextSurface errorscreen(errotext,fontbig,fontcolor);

// search for apks
    vector<AndroidApkEx*> apks;

    if (list_apks(APKFOLDER,&apks)>0)
    {
        // initialize their icons
        init_widgets(apks,fontsmall);
        // align icons
        align_widgets(screen,apks);
    }


//main loop
    AndroidApkEx* runapk = NULL;
    AndroidApkEx* tmpapk = NULL;

    bool done = false;
    while (!done && !runapk)
    {
        SDL_BlitSurface(background,0,screen,0);

        if (apks.size())
            draw_widgets(screen,apks);
        else
            errorscreen.blit_to(screen);

        SDL_Flip(screen);

// using waitevent not poll ... no per-frame updated needed
        SDL_Event event;
        if (SDL_WaitEvent(&event))
        {
            switch (event.type)
            {
            case SDL_QUIT:
                done = true;
                break;

            case SDL_KEYDOWN:
                if (event.key.keysym.sym == SDLK_ESCAPE)
                    done = true;
                break;

            case SDL_MOUSEBUTTONDOWN:
                tmpapk = select_apk(event.button.x,event.button.y,apks);
                if (tmpapk) {
                    cout << "Selected " << tmpapk->get_apk_filename() << endl;
                    runapk = tmpapk;
                }
                break;
            }
        }
    }

    SDL_Quit();
    TTF_Quit();

    if (runapk)
    {
        string cmdline = RUNAPK;
        cmdline += " \"" + runapk->get_apk_filename() +"\"";
        cmdline += " \"" + string(argv[0]) +"\"";
        cout << "cmdline=" << cmdline << endl;
        system(cmdline.c_str());
    }

    return 0;
}
