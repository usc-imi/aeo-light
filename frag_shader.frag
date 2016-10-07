#version 120
varying  vec4 col;
varying  vec2 vTexCoord;
uniform sampler2D frame_tex;
uniform sampler2D adj_frame_tex;
uniform sampler2D prev_frame_tex;
uniform sampler2D audio_tex;
uniform sampler2D prev_audio_tex;
uniform sampler2D overlap_audio_tex;
uniform sampler2D overlapcompute_audio_tex;
uniform sampler2D cal_audio_tex;
uniform float overlapshow;
uniform float render_mode;
uniform float show_mode;
uniform float overlap_target;
uniform float isstereo;
uniform float negative;
uniform vec2 inputsize; //input size x y int pixels
uniform vec2 dminmax;
uniform vec2 pix_boundry;
uniform vec4 bounds; //x1,x2,y1,y2
uniform vec4 color_controls; //lift, gamma, gain,sat;
uniform vec3 manip_controls; //thresh,threshold amount,  blur
uniform vec4 overlap; //   y_search Area ,y_ offset,  bottom,  top;
uniform vec4 cal_controls;
//out int ucol;


vec3 RGBToHSL(vec3 color)
{
    vec3 hsl; // init to 0 to avoid warnings ? (and reverse if + remove first part)

    float fmin = min(min(color.r, color.g), color.b);    //Min. value of RGB
    float fmax = max(max(color.r, color.g), color.b);    //Max. value of RGB
    float delta = fmax - fmin;             //Delta RGB value

    hsl.z = (fmax + fmin) / 2.0; // Luminance

    if (delta == 0.0)		//This is a gray, no chroma...
    {
        hsl.x = 0.0;	// Hue
        hsl.y = 0.0;	// Saturation
    }
    else                                    //Chromatic data...
    {
        if (hsl.z < 0.5)
            hsl.y = delta / (fmax + fmin); // Saturation
        else
            hsl.y = delta / (2.0 - fmax - fmin); // Saturation

        float deltaR = (((fmax - color.r) / 6.0) + (delta / 2.0)) / delta;
        float deltaG = (((fmax - color.g) / 6.0) + (delta / 2.0)) / delta;
        float deltaB = (((fmax - color.b) / 6.0) + (delta / 2.0)) / delta;

        if (color.r == fmax )
            hsl.x = deltaB - deltaG; // Hue
        else if (color.g == fmax)
            hsl.x = (1.0 / 3.0) + deltaR - deltaB; // Hue
        else if (color.b == fmax)
            hsl.x = (2.0 / 3.0) + deltaG - deltaR; // Hue

        if (hsl.x < 0.0)
            hsl.x += 1.0; // Hue
        else if (hsl.x > 1.0)
            hsl.x -= 1.0; // Hue
    }

    return hsl;
}

float HueToRGB(float f1, float f2, float hue)
{
    if (hue < 0.0)
        hue += 1.0;
    else if (hue > 1.0)
        hue -= 1.0;
    float res;
    if ((6.0 * hue) < 1.0)
        res = f1 + (f2 - f1) * 6.0 * hue;
    else if ((2.0 * hue) < 1.0)
        res = f2;
    else if ((3.0 * hue) < 2.0)
        res = f1 + (f2 - f1) * ((2.0 / 3.0) - hue) * 6.0;
    else
        res = f1;
    return res;
}

vec3 HSLToRGB(vec3 hsl)
{
    vec3 rgb;

    if (hsl.y == 0.0)
        rgb = vec3(hsl.z); // Luminance
    else
    {
        float f2;

        if (hsl.z < 0.5)
            f2 = hsl.z * (1.0 + hsl.y);
        else
            f2 = (hsl.z + hsl.y) - (hsl.y * hsl.z);

        float f1 = 2.0 * hsl.z - f2;

        rgb.r = HueToRGB(f1, f2, hsl.x + (1.0/3.0));
        rgb.g = HueToRGB(f1, f2, hsl.x);
        rgb.b= HueToRGB(f1, f2, hsl.x - (1.0/3.0));
    }

    return rgb;
}
























void main(void)
{

    vec2 flip_coord = vec2 (vTexCoord.x,1.0- vTexCoord.y);


    float dxStep= (1.0/inputsize.x)*2.0;
    float dyStep= (1.0/inputsize.y)*2.0;

    float KERNEL_HSHARPEN [25];

    KERNEL_HSHARPEN [0]=-0.125;
    KERNEL_HSHARPEN [1]=-0.125;
    KERNEL_HSHARPEN [2]=-0.125;
    KERNEL_HSHARPEN [3]=-0.125;
    KERNEL_HSHARPEN [4]=-0.125;
    KERNEL_HSHARPEN [5]=-0.125;
    KERNEL_HSHARPEN [6]=0.25;
    KERNEL_HSHARPEN [7]=0.25;
    KERNEL_HSHARPEN [8]=0.25;

    KERNEL_HSHARPEN [9]=-0.125;
    KERNEL_HSHARPEN [10]=-0.125;
    KERNEL_HSHARPEN [11]=0.25;
    KERNEL_HSHARPEN [12]=1.0;
    KERNEL_HSHARPEN [13]=0.25;
    KERNEL_HSHARPEN [14]=-0.125;
    KERNEL_HSHARPEN [15]=-0.125;
    KERNEL_HSHARPEN [16]=0.25;
    KERNEL_HSHARPEN [17]=0.25;


    KERNEL_HSHARPEN [18]=0.25;
    KERNEL_HSHARPEN [19]=-0.125;
    KERNEL_HSHARPEN [20]=-0.125;
    KERNEL_HSHARPEN [21]=-0.125;
    KERNEL_HSHARPEN [22]=-0.125;
    KERNEL_HSHARPEN [23]=-0.125;
    KERNEL_HSHARPEN [24]=-0.125;
    //Gaussian blur
    float KERNEL_HBLUR [25] = float [] (

            0.03252263605892945, 0.03778591223705376, 0.0397232373850157, 0.03778591223705376, 0.03252263605892945,
                               0.03778591223705376, 0.04390096672973462, 0.04615181742593543, 0.04390096672973462, 0.03778591223705376,
                               0.0397232373850157, 0.04615181742593543, 0.04851807170510919, 0.04615181742593543, 0.0397232373850157,
                               0.03778591223705376, 0.04390096672973462, 0.04615181742593543, 0.04390096672973462, 0.03778591223705376,
                               0.03252263605892945, 0.03778591223705376, 0.0397232373850157, 0.03778591223705376, 0.03252263605892945

                               );




    vec2 ioffset[25];


    ioffset[0] = vec2(-2.0 * dxStep,2.0*  dyStep);
    ioffset[1] = vec2(-dxStep,2.0*  dyStep);
    ioffset[2] = vec2(0.0,   2.0*  dyStep);
    ioffset[3] = vec2(dxStep, 2.0*  dyStep);
    ioffset[4] = vec2(2.0 * dxStep, 2.0*  dyStep);

    ioffset[5] = vec2(-2.0 * dxStep, dyStep);
    ioffset[6] = vec2(-dxStep,dyStep);
    ioffset[7] = vec2(0.0, dyStep);
    ioffset[8] = vec2( dxStep,dyStep);
    ioffset[9] = vec2(2.0* dxStep, dyStep);


    ioffset[10] = vec2(-2.0 * dxStep, 0.0);
    ioffset[11] = vec2(-dxStep, 0.0);
    ioffset[12] = vec2(0.0, 0.0);
    ioffset[13] = vec2( dxStep, 0.0);
    ioffset[14] = vec2(2.0 *dxStep, 0.0);


    ioffset[15] = vec2(-2.0 * dxStep, -dyStep);
    ioffset[16] = vec2(-dxStep, -dyStep);
    ioffset[17] = vec2(0.0, -dyStep);
    ioffset[18] = vec2(dxStep, dyStep);
    ioffset[19] = vec2(2.0* dxStep, -dyStep);

    ioffset[20] = vec2(-2.0* dxStep,-2.0*  dyStep);
    ioffset[21] = vec2(-dxStep,-2.0*  dyStep);
    ioffset[22] = vec2(0.0, -2.0*  dyStep);
    ioffset[23] = vec2(dxStep,-2.0*  dyStep);
    ioffset[24] = vec2(2.0* dxStep, -2.0*  dyStep);

    vec4 texel =vec4(0);

    if (render_mode==0.0)    //// first pass corrections render
    {

        vec4 sharp_texel =vec4(0);
        vec4 blur_texel =vec4(0);
        vec4 tmps =vec4(0);
        int k;
        vec2 grabberm;
        vec2 grabber;
        texel = texture2D(frame_tex, vTexCoord);
        if(cal_controls.y==1.0)
        {
            grabberm =  vec2(4.0,4.0);
        }
        else
        {
            grabberm =vec2(1.0);
        }

        if( vTexCoord.x < bounds.y && vTexCoord.x > bounds.x)

        {
            for( k=0; k<25; k++ )
            {


                grabber = vTexCoord + (ioffset[k]*grabberm);
                grabber.y= max(0.0,min(grabber.y,1.0));



                tmps = texture2D(frame_tex,grabber);

                sharp_texel+=tmps*KERNEL_HSHARPEN [k];
                blur_texel+=(tmps *KERNEL_HBLUR [k]);
            }

            if(cal_controls.y==0.0)
            {
                if(manip_controls.z>=0.0)
                    texel = mix(texel,sharp_texel,manip_controls.z);  //sharpen control
                else
                    texel = mix(texel,blur_texel,(0.0-manip_controls.z)*2.0); //blur control

                if(cal_controls.x==1.0)
                {
                    vec4        cal_texel=vec4(0.0);


                    cal_texel =  texture2D(cal_audio_tex, vec2(.25,1.0-vTexCoord.y));
                    texel*=vec4(0.5/cal_texel.x);


                }


                if(negative==1.0)
                    texel=vec4(1.0)-texel;//neg flip
                texel+=vec4(color_controls.x); //lift should chnage to neg gain?
                texel=clamp(texel,vec4(0),vec4(1.0));
                texel=pow(texel,vec4(color_controls.y)); //gamma
                texel*=vec4(color_controls.z);           //gain
                texel=clamp(texel,vec4(0),vec4(1.0));
                texel.xyz=RGBToHSL(texel.xyz);
                texel.y  *= color_controls.a; //saturation

                texel.xyz=HSLToRGB(texel.xyz);

                if (manip_controls.x==1.0)//threshold boolean
                {

                    texel = pow(texel,vec4(manip_controls.y)) / ((pow(vec4(0.5),vec4(manip_controls.y))) +pow(texel,vec4(manip_controls.y)) ); //sigmoid function


                }
            }
            else
            {

                texel=blur_texel;

            }

        }





    }
    if (render_mode==1.0) //sound render internal for display purposes
    {
        texel = vec4(0);
        vec4 ptex = vec4(0);
        float samplesperline = 2048.0;
        float trackwidth=  bounds.y -bounds.x;

        float track_iter= trackwidth/1024.0;
        if (isstereo ==2.0)
            track_iter/=2.0;  //Push pull one half ;
        for(int i =0 ; i<1024; i++)
        {


            texel += texture2D(adj_frame_tex, vec2(bounds.x+(float(i)*track_iter),1.0-vTexCoord.y)); //steps through track bounded line 1024 times
            ptex  += texture2D(prev_frame_tex, vec2(bounds.x+(float(i)*track_iter),overlap.a- 1.0-vTexCoord.y)); //steps through track bounded line 1024 times
        }

        texel/=vec4(1024.0);
        texel = vec4(RGBToHSL(texel.xyz).z);

        ptex/=vec4(1024.0);
        ptex = vec4(RGBToHSL(texel.xyz).z);


        // tonegenerator       texel=vec4(sin(3.14159*100*(1.0-vTexCoord.y)) +1.0)/2.0;
    }
    if (render_mode==1.5)   //sounder render for file output
    {
        texel = vec4(0);
        vec4 ptex = vec4(0);
        vec4 ctex = vec4(0);
        float samplesperline = 2048.0;  // 2048 samples inside track defined area
        float trackwidth=  bounds.y -bounds.x;
        float track_iter= trackwidth/samplesperline;
        float ypblend =0;

        if (isstereo ==0.0) //is stereo output ?
        {
            ypblend = (vTexCoord.y -(1.0-overlap.x) );// - (1.0 + (overlap.z - overlap.x)   +overlap.a );
            for(int i =0 ; i<int(samplesperline); i++)
            {


                texel += texture2D(prev_frame_tex, vec2(bounds.x+(float(i)*track_iter),vTexCoord.y)); //current frame sound
                ptex += texture2D(adj_frame_tex, vec2(bounds.x+(float(i)*track_iter), ypblend )); //prev frame sound



            }



        }

        else //mono output
        {
            samplesperline = samplesperline/2.0;
            if (flip_coord.x<(bounds.x+(trackwidth/2.0)))
            {
                for(int i =0 ; i<int(samplesperline); i++)
                {


                    texel += texture2D(prev_frame_tex, vec2(bounds.x+(float(i)*track_iter),1.0-flip_coord.y));//current frame sound
                    ptex += texture2D(adj_frame_tex, vec2(bounds.x+(float(i)*track_iter), ypblend ));//prev frame sound


                }

            }
            else
            {
                for(int i =0 ; i<int(samplesperline); i++)
                {


                    texel += texture2D(prev_frame_tex, vec2((bounds.x+(trackwidth/2.0))+(float(i)*track_iter),1.0-flip_coord.y));//current frame sound
                    ptex += texture2D(adj_frame_tex, vec2(bounds.x+(float(i)*track_iter), ypblend ));//prev frame sound


                }

            }

        }
        if (overlap.a - ypblend <= 0.01 && ypblend>0.0)//overlap blend 1% take prev and curr and blend linear scale over overlap
        {

            texel =mix(ptex,texel,(overlap.a - ypblend)*100.0);
        }
        texel/=vec4(samplesperline);
        texel= pow(texel,vec4(1.0));
        texel = vec4(RGBToHSL(texel.xyz).z); //convert to luminance



    }
    if (render_mode==4.0) //sound ovelrap render with pix match to 1d array
    {

        float trackwidth=  bounds.y -bounds.x;    //sound area
        float trackwidthpix=  pix_boundry.y -pix_boundry.x; //pix area
        float track_iterpix= trackwidthpix/1024.0;
        float track_iter= trackwidth/1024.0;
        if (isstereo ==2.0)
            track_iter/=2.0;  //Push pull one half ;
        vec4 qtexel = vec4(0);
        vec4 pqtexel = vec4(0);
        texel = vec4(0);
        vec4 pix_texel = vec4(0);


        if (flip_coord.x<0.5) //current frame ouput 1d compare array
        {
            for(int i =0 ; i<1024; i++)
            {

                texel +=( texture2D(adj_frame_tex, vec2(bounds.x+(float(i)*track_iter), vTexCoord.y))   * vec4(1.0/1024.0 ));   //column0 curr frame sound

                pix_texel +=( texture2D(adj_frame_tex, vec2(pix_boundry.x+(float(i)*track_iterpix), vTexCoord.y))   * vec4(1.0/1024.0 ));   //column0 curr frame sound





            }
            if (overlap_target ==1.0)
                texel=pix_texel;
            if (overlap_target == 2.0)
                texel= (texel + pix_texel)/2.0;

            texel -= vec4(dminmax.x);
            texel *= vec4(1.0/(dminmax.y-dminmax.x));  // auto range based off of min and max values detected on current frame and apply to previous
        }
        else  //previous frame ouput 1d compare array
        {
            for(int i =0 ; i<1024; i++)
            {

                texel+= ((texture2D(prev_frame_tex, vec2(bounds.x+(float(i)*track_iter),vTexCoord.y))) *vec4(1.0/1024.0 ));      //column1 previous frame pix
                pix_texel+= ((texture2D(prev_frame_tex, vec2(pix_boundry.x+(float(i)*track_iterpix),vTexCoord.y))) *vec4(1.0/1024.0 ));      //column1 previous frame pix


            }

            if (overlap_target == 1.0)
                texel=pix_texel;
            if (overlap_target == 2.0)
                texel= (texel + pix_texel)/2.0;


            texel -= vec4(dminmax.x);
            texel *= vec4(1.0/(dminmax.y-dminmax.x));
        }



        texel = vec4(RGBToHSL(texel.xyz).z); //convert to luminance

    }



    if (render_mode==5.0) //  2d overlap invert and add xnumber of pixels contained in y overlap over x sampels
    {
        float loc= (flip_coord.y*1000.0);
        vec4 weighter = vec4(0.0);
        int realsamp = 0;
        int samp=0;
        vec2 ss =vec2( (1.0- vTexCoord.x),0.0);

        samp=int(.25 *(inputsize.y*2.0)); //1 pixel movements inside overlap area 25% OVERLAP MAX

        float sampstep= (1.0/(inputsize.y)); // pixel float 1/y size

        loc *= 1.0 / inputsize.y;





        //agc area **********************
        float min =1.0;
        float max =0.0;


        float prevs=(0.0);
        float currs=(0.0);
        texel=vec4(0.0);
        for(int i =0 ; i<int(samp); i++) //shift i pixels then  sum differnces of 1d sound or sound & pix as previously rendered previous vs current
        {

            prevs=((((( texture2D(overlap_audio_tex, vec2(0.25,(float(i)*sampstep)))))).x));
            currs = ((((( texture2D(overlap_audio_tex, vec2(0.75,(float(i)*sampstep)+1.0-flip_coord.y))))).x));


            if (float(i)*sampstep<1.0 && float(i)*sampstep>0.0 ) //is a legit on screen sample ?
            {
                texel+= vec4(abs(prevs-currs));
                realsamp++;
            }

        }
        texel/= vec4(realsamp);  //if sampling goes off screen ignore value values

        weighter = texel; // parabolic roll off of probablity working away from overlap calculated value

        if ((1.0- vTexCoord.y )>= overlap.z+overlap.a-overlap.y && (1.0- vTexCoord.y )<=overlap.z+overlap.a)
            weighter *= vec4(1.0)+ vec4(0.5*(1.0 - smoothstep(overlap.z+overlap.a-overlap.y,overlap.z+overlap.a,(1.0- vTexCoord.y ))));
        if ((1.0- vTexCoord.y ) <= overlap.z+overlap.y+overlap.a && (1.0- vTexCoord.y )>=overlap.z+overlap.a)//
            weighter *=  vec4(1.0)+vec4(0.5* smoothstep(overlap.z+overlap.a,overlap.z+overlap.a+overlap.y,(1.0- vTexCoord.y )));

        texel = mix(texel,weighter,vec4(0.5));  //weight error by position


        // if outside search area set error to max error( 1.0)

        if ((1.0- vTexCoord.y )> overlap.z+overlap.a+overlap.y)
            texel=vec4(1.0);

        if ((1.0- vTexCoord.y )< overlap.a +overlap.z-overlap.y)
            texel=vec4(1.0);


    }



    if (render_mode==2.0) ////to screen picture render
    {
        texel = texture2D(adj_frame_tex, flip_coord    ); //adjusted picture

        if((  abs(flip_coord.y -overlap.a ) < overlap.y)    )
        {

            float y_lap;

            float compute_h=0.0;

            if (overlapshow==1.0)
            {
                texel+= (texture2D(prev_frame_tex, flip_coord +vec2(0,1.0-overlap.x)  ));

                texel/=vec4(2.0);


                if(( flip_coord +vec2(0,1.0-overlap.x)).y>1.0)
                    texel=vec4(0);
            }

            if(  abs(flip_coord.y -overlap.a ) <0.0025)
            {

                texel=vec4(1,1,1,0);  //top line

            }


        }


        if( 2.0*(abs(vTexCoord.y- overlap.z) )<overlap.y)
        {
            texel = texture2D(adj_frame_tex, flip_coord    );
            texel+=vec4(0.25);

            float comph=1.0-(flip_coord +vec2(0,1.0-overlap.x)  ).y;

            if(  abs(vTexCoord.y- (overlap.x-overlap.a)) <0.0025)
            {

                texel=vec4(1,0,1,0);

            }
        }
        if(  abs(flip_coord.y -overlap.a ) <0.0025)//.a abs(vTexCoord.x-overlap.x)<0.004
        {

            texel=vec4(1,1,1,0);

        }

        if (show_mode==0.0)
        {
            float trackwidth = bounds.y-bounds.x;
            if (abs(vTexCoord.x - bounds.x )   <0.002)  //left sound line boundry
                texel=vec4(1,0,1,0);
            if (abs(vTexCoord.x - bounds.y )   <0.002)  //right sound line boundry
                texel=vec4(1,0,0,0);
            if (isstereo > 0.0)
            {
                if (abs(vTexCoord.x -( bounds.x + trackwidth/2.0) )   <0.002    && mod(int(vTexCoord.y*100.0),2)==0 )  // septum indicator
                    texel=vec4(1,1,1,0);
            }
            if (abs(vTexCoord.x - pix_boundry.x )   <0.002) //right pix line boundry
                texel=vec4(1,1,0,0);
            if (abs(vTexCoord.x - pix_boundry.y )   <0.002) //right pix line boundry
                texel=vec4(1,1,0,0);

        }
    }

    if (render_mode==3.0) ////sound to screen
    {
        texel =  texture2D(audio_tex, vec2(.25,1.0-vTexCoord.x));
        vec4 prev_texel = texture2D(prev_audio_tex, vec2(.25,1.0-flip_coord.x)- vec2(0,1.0-overlap.x));
        texel=vec4(texel.x);
        prev_texel=vec4(prev_texel.x);





        if (vTexCoord.y<0.5)             //waveform no loop beacuse 1d
        {
            texel = texture2D(audio_tex, vec2(.25,1.0-vTexCoord.x));
            vec4 cal_texel =  texture2D(cal_audio_tex, vec2(.25,1.0-vTexCoord.x));

            texel=vec4(texel.x);


            float ypos = vTexCoord.y*2.0;

            // curr frame wfm
            if(abs(texel.x -ypos) <0.0075)
            {
                texel=vec4(0,1,0,0);
            }
            else
                texel=vec4(0,0,0,0);

            // prev frame wfm
            if(abs(prev_texel.x -ypos) <0.0075)
            {
                if(vTexCoord.x<overlap.x)
                    texel+=vec4(1,0,0,0);
            }

            // cal signal wfm
            if(abs(cal_texel.x -ypos) <0.0075)
            {

                texel+=vec4(0,0,1,0);
            }

            //overlap area highlight WFM
            if  (abs(vTexCoord.x - overlap.a) < (overlap.y)/2.0)
            {

                texel+=vec4(0,0.2,0,0);
            }

            //overlap line indicator WFM
            if( abs( vTexCoord.x-overlap.a) <0.004 && ypos >0.9)
            {
                texel+=vec4(1,0,1,0);
            }

        }


        if (vTexCoord.y>0.85&& vTexCoord.y<0.90)//prev frame overlap density
        {


            texel=vec4((((( texture2D(overlap_audio_tex, vec2(.75,vTexCoord.x+1.0-overlap.x))))).x));
            if(  (vTexCoord.x) >0.25   )
                texel=vec4(0,0,0,0);
        }



        if (vTexCoord.y>0.90&& vTexCoord.y<0.95)//curr frame overlap density
        {
            texel=vec4((((( texture2D(overlap_audio_tex, vec2(.25,vTexCoord.x))))).x));


        }
        if( vTexCoord.y>0.95)  // computed overlap error display
        {

            texel=vec4((((( texture2D(overlapcompute_audio_tex, vec2(.25,1.0-(vTexCoord.x)))))).x));

        }
        if( vTexCoord.y>0.98 && abs(vTexCoord.x-overlap.x)<0.004) //overlap marker
        {

            texel=vec4(1,0,0,0);

        }
    }




    gl_FragColor = vec4(texel.xyz,0.005); //alpha is ignored except for calibration blend


}
