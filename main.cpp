#include <cstdlib>
#include <SDL.h>
#include <SDL/SDL_image.h>
#include <vector>
#include <string>
#include <dirent.h>
#include <unistd.h>
#include <sys/param.h>
#include <strings.h>
#include <iostream>
#include <sys/stat.h>

#define SCREEN_WIDTH  800
#define SCREEN_HEIGHT 480
#define SCREEN_BITS   16
#define ICONBORDER    8
#ifdef PANDORA
#define SDL_VIDEOMODE SDL_SWSURFACE|SDL_FULLSCREEN|SDL_DOUBLEBUF
#else
#define SDL_VIDEOMODE SDL_HWSURFACE|SDL_DOUBLEBUF
#endif

// folder where to find the apks
#define APKFOLDER "./apks"
// folder where to install cached icons
#define ICONCACHEFOLDER "./.apkenvui"


extern "C" {
#include "../apkenv/apklib/apklib.h"
}

using namespace std;


class AndroidApkEx
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
                    FILE *fp = fopen(S_CurrentApk->apk_icon.c_str(),"wb");
                    if (fp)
                    {
                        fwrite(buf,size,1,fp);
                        fclose(fp);
                    }
                }
            }
        }
    }


    AndroidApkEx( const string& folder, const string& name ) :
        apk_folder(folder),
        apk_name(name),
        sdl_icon(NULL)
    {
        apk_filepath = folder+"/"+name;
        apk = apk_open(apk_filepath.c_str());
        apk_icon = string(ICONCACHEFOLDER) + "/" + name + ".png";
        memset(&drawrect,0,sizeof(drawrect));
    }

    ~AndroidApkEx()
    {
        if (apk)
            apk_close(apk);
    }


    string get_apk_filename() const
    {
        return apk_filepath;
    }

    /** icon **/

    void extract_icon()
    {
        const char* iconfolders[] = {
            "res/drawable-hdpi/",
            "res/drawable",
            0
        };

        S_CurrentApk = this;
        int i=0;
        while(!icon_exists() && iconfolders[i])
        {
            apk_for_each_file(apk,iconfolders[i],extract_icon_callback);
            i++;
        }
        S_CurrentApk = NULL;
    }
    bool icon_exists()
    {
        struct stat st;
        if (stat(apk_icon.c_str(),&st)>=0)
            return true;
        return false;
    }

    bool init_icon_surface()
    {
        if (icon_exists() && sdl_icon==NULL)
        {
            sdl_icon = IMG_Load(apk_icon.c_str());
        }

        return sdl_icon!=NULL;
    }

    SDL_Surface* get_icon_surface() const
    {
        return sdl_icon;
    }

    void set_draw_rect( const SDL_Rect& rect )
    {
        drawrect = rect;
        drawrect.x = drawrect.x + (rect.w-sdl_icon->w)/2;
        drawrect.y = drawrect.y + (rect.h-sdl_icon->h)/2;
    }
    SDL_Rect* get_draw_rect()
    {
        return &drawrect;
    }

    operator AndroidApk*()
    {
        return apk;
    }


private:
    AndroidApk* apk;
    string apk_filepath;
    string apk_folder;
    string apk_name;
    string apk_icon;
    SDL_Surface* sdl_icon;
    SDL_Rect drawrect;
};
AndroidApkEx*  AndroidApkEx::S_CurrentApk = 0;

///
int list_apks( const char* directory, vector<AndroidApkEx*>* apks )
{
    DIR* dir = opendir( directory );
    if (!dir)
        return 0;

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


void init_icons(const vector<AndroidApkEx*>& apks)
{
    for (int i=0,n=apks.size(); i<n; i++ ) {
        apks[i]->extract_icon();
        if (!apks[i]->init_icon_surface()) {
            cerr << "Failed to load Icon for " << apks[i]->get_apk_filename() << endl;
        } else {
            cerr << "Icon loaded for " << apks[i]->get_apk_filename() << endl;
        }
    }
}

void align_icons(SDL_Surface *target, int border, const vector<AndroidApkEx*>& apks)
{
    int maxwidth = 0;
    int maxheight = 0;

    for (int i=0,n=apks.size(); i<n; i++ ) {
        AndroidApkEx* apk = apks[i];
        SDL_Surface* icon = apk->get_icon_surface();

        if (icon) {
            if (icon->w>maxwidth) maxwidth = icon->w + border * 2;
            if (icon->h>maxheight) maxheight = icon->h + border * 2;
        }
    }

    SDL_Rect rect = {border,border,maxwidth,maxheight};


    for (int i=0,n=apks.size(); i<n; i++ ) {

        apks[i]->set_draw_rect(rect);

        rect.x += maxwidth;
        if (rect.x>=target->w)
        {
            rect.x = border;
            rect.y += maxheight;
        }
    }
}

void draw_icons(SDL_Surface *target, const vector<AndroidApkEx*>& apks)
{
    for (int i=0,n=apks.size(); i<n; i++ ) {
        AndroidApkEx* apk = apks[i];
        SDL_Surface* icon = apk->get_icon_surface();

        if (icon) {
            SDL_BlitSurface(icon,NULL,target,apk->get_draw_rect());
        }
    }
}

int main ( int argc, char** argv )
{
    // initialize SDL video
    if ( SDL_Init( SDL_INIT_VIDEO|SDL_INIT_JOYSTICK ) < 0 )
    {
        printf( "Unable to init SDL: %s\n", SDL_GetError() );
        return 1;
    }

    // make sure SDL cleans up before exit
    atexit(SDL_Quit);

    // create a new window
    SDL_Surface* screen = SDL_SetVideoMode(SCREEN_WIDTH, SCREEN_HEIGHT, SCREEN_BITS, SDL_VIDEOMODE);
    if ( !screen )
    {
        printf("Unable to set %dx%d video: %s\n", SCREEN_WIDTH,SCREEN_HEIGHT,SDL_GetError());
        return 1;
    }


    vector<AndroidApkEx*> apks;

    // search for apks
    list_apks(APKFOLDER,&apks);

    // initialize their icons
    init_icons(apks);

    // align icons
    align_icons(screen,ICONBORDER,apks);


    // program main loop
    bool done = false;
    while (!done)
    {
        SDL_Event event;
        while (SDL_PollEvent(&event))
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
            }
        }


        SDL_FillRect(screen, 0, SDL_MapRGB(screen->format, 64, 64, 64));

        draw_icons(screen,apks);

        SDL_Flip(screen);
    }


    return 0;
}
