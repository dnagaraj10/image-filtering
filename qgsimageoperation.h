 #ifndef QGSIMAGEOPERATION_H
 #define QGSIMAGEOPERATION_H

 #include <QImage>
 #include "qgis_sip.h"
 #include <QColor>

 #include "qgis_core.h"
 #include <cmath>

 class QgsColorRamp;

 class CORE_EXPORT QgsImageOperation
 {

   public:

     enum GrayscaleMode
     {
       GrayscaleLightness,
       GrayscaleLuminosity,
       GrayscaleAverage,
       GrayscaleOff
     };

     enum FlipType
     {
       FlipHorizontal,
       FlipVertical
     };

     static void convertToGrayscale( QImage &image, GrayscaleMode mode = GrayscaleLuminosity );

     static void adjustBrightnessContrast( QImage &image, int brightness, double contrast );

     static void adjustHueSaturation( QImage &image, double saturation, const QColor &colorizeColor = QColor(),
                                      double colorizeStrength = 1.0 );

     static void multiplyOpacity( QImage &image, double factor );

     static void overlayColor( QImage &image, const QColor &color );

     struct DistanceTransformProperties
     {

       bool shadeExterior = true;

       bool useMaxDistance = true;

       double spread = 10.0;

       QgsColorRamp *ramp = nullptr;
     };

     static void distanceTransform( QImage &image, const QgsImageOperation::DistanceTransformProperties &properties );

     static void stackBlur( QImage &image, int radius, bool alphaOnly = false );

     static QImage *gaussianBlur( QImage &image, int radius ) SIP_FACTORY;

     static void flipImage( QImage &image, FlipType type );

     static QRect nonTransparentImageRect( const QImage &image, QSize minSize = QSize(), bool center = false );

     static QImage cropTransparent( const QImage &image, QSize minSize = QSize(), bool center = false );

   private:

     //for blocked operations
     enum LineOperationDirection
     {
       ByRow,
       ByColumn
     };
     template <class BlockOperation> static void runBlockOperationInThreads( QImage &image, BlockOperation &operation, LineOperationDirection direction );
     struct ImageBlock
     {
       unsigned int beginLine;
       unsigned int endLine;
       unsigned int lineLength;
       QImage *image = nullptr;
     };

     //for rect operations
     template <typename RectOperation> static void runRectOperation( QImage &image, RectOperation &operation );
     template <class RectOperation> static void runRectOperationOnWholeImage( QImage &image, RectOperation &operation );

     //for per pixel operations
     template <class PixelOperation> static void runPixelOperation( QImage &image, PixelOperation &operation );
     template <class PixelOperation> static void runPixelOperationOnWholeImage( QImage &image, PixelOperation &operation );
     template <class PixelOperation>
     struct ProcessBlockUsingPixelOperation
     {
       explicit ProcessBlockUsingPixelOperation( PixelOperation &operation )
         : mOperation( operation ) { }

       typedef void result_type;

       void operator()( ImageBlock &block )
       {
         for ( unsigned int y = block.beginLine; y < block.endLine; ++y )
         {
           QRgb *ref = reinterpret_cast< QRgb * >( block.image->scanLine( y ) );
           for ( unsigned int x = 0; x < block.lineLength; ++x )
           {
             mOperation( ref[x], x, y );
           }
         }
       }

       PixelOperation &mOperation;
     };

     //for linear operations
     template <typename LineOperation> static void runLineOperation( QImage &image, LineOperation &operation );
     template <class LineOperation> static void runLineOperationOnWholeImage( QImage &image, LineOperation &operation );
     template <class LineOperation>
     struct ProcessBlockUsingLineOperation
     {
       explicit ProcessBlockUsingLineOperation( LineOperation &operation )
         : mOperation( operation ) { }

       typedef void result_type;

       void operator()( ImageBlock &block )
       {
         //do something with whole lines
         int bpl = block.image->bytesPerLine();
         if ( mOperation.direction() == ByRow )
         {
           for ( unsigned int y = block.beginLine; y < block.endLine; ++y )
           {
             QRgb *ref = reinterpret_cast< QRgb * >( block.image->scanLine( y ) );
             mOperation( ref, block.lineLength, bpl );
           }
         }
         else
         {
           //by column
           unsigned char *ref = block.image->scanLine( 0 ) + 4 * block.beginLine;
           for ( unsigned int x = block.beginLine; x < block.endLine; ++x, ref += 4 )
           {
             mOperation( reinterpret_cast< QRgb * >( ref ), block.lineLength, bpl );
           }
         }
       }

       LineOperation &mOperation;
     };


     //individual operation implementations

     class GrayscalePixelOperation
     {
       public:
         explicit GrayscalePixelOperation( const GrayscaleMode mode )
           : mMode( mode )
         {  }

         void operator()( QRgb &rgb, int x, int y );

       private:
         GrayscaleMode mMode;
     };
     static void grayscaleLightnessOp( QRgb &rgb );
     static void grayscaleLuminosityOp( QRgb &rgb );
     static void grayscaleAverageOp( QRgb &rgb );


     class BrightnessContrastPixelOperation
     {
       public:
         BrightnessContrastPixelOperation( const int brightness, const double contrast )
           : mBrightness( brightness )
           , mContrast( contrast )
         {  }

         void operator()( QRgb &rgb, int x, int y );

       private:
         int mBrightness;
         double mContrast;
     };


     class HueSaturationPixelOperation
     {
       public:
         HueSaturationPixelOperation( const double saturation, const bool colorize,
                                      const int colorizeHue, const int colorizeSaturation,
                                      const double colorizeStrength )
           : mSaturation( saturation )
           , mColorize( colorize )
           , mColorizeHue( colorizeHue )
           , mColorizeSaturation( colorizeSaturation )
           , mColorizeStrength( colorizeStrength )
         {  }

         void operator()( QRgb &rgb, int x, int y );

       private:
         double mSaturation; // [0, 2], 1 = no change
         bool mColorize;
         int mColorizeHue;
         int mColorizeSaturation;
         double mColorizeStrength; // [0,1]
     };
     static int adjustColorComponent( int colorComponent, int brightness, double contrastFactor );


     class MultiplyOpacityPixelOperation
     {
       public:
         explicit MultiplyOpacityPixelOperation( const double factor )
           : mFactor( factor )
         { }

         void operator()( QRgb &rgb, int x, int y );

       private:
         double mFactor;
     };

     class ConvertToArrayPixelOperation
     {
       public:
         ConvertToArrayPixelOperation( const int width, double *array, const bool exterior = true )
           : mWidth( width )
           , mArray( array )
           , mExterior( exterior )
         {
         }

         void operator()( QRgb &rgb, int x, int y );

       private:
         int mWidth;
         double *mArray = nullptr;
         bool mExterior;
     };

     class ShadeFromArrayOperation
     {
       public:
         ShadeFromArrayOperation( const int width, double *array, const double spread,
                                  const DistanceTransformProperties &properties )
           : mWidth( width )
           , mArray( array )
           , mSpread( spread )
           , mProperties( properties )
         {
           mSpreadSquared = std::pow( mSpread, 2.0 );
         }

         void operator()( QRgb &rgb, int x, int y );

       private:
         int mWidth;
         double *mArray = nullptr;
         double mSpread;
         double mSpreadSquared;
         const DistanceTransformProperties &mProperties;
     };
     static void distanceTransform2d( double *im, int width, int height );
     static void distanceTransform1d( double *f, int n, int *v, double *z, double *d );
     static double maxValueInDistanceTransformArray( const double *array, unsigned int size );


     class StackBlurLineOperation
     {
       public:
         StackBlurLineOperation( int alpha, LineOperationDirection direction, bool forwardDirection, int i1, int i2 )
           : mAlpha( alpha )
           , mDirection( direction )
           , mForwardDirection( forwardDirection )
           , mi1( i1 )
           , mi2( i2 )
         { }

         typedef void result_type;

         LineOperationDirection direction() { return mDirection; }

         void operator()( QRgb *startRef, int lineLength, int bytesPerLine )
         {
           unsigned char *p = reinterpret_cast< unsigned char * >( startRef );
           int rgba[4];
           int increment = ( mDirection == QgsImageOperation::ByRow ) ? 4 : bytesPerLine;
           if ( !mForwardDirection )
           {
             p += ( lineLength - 1 ) * increment;
             increment = -increment;
           }

           for ( int i = mi1; i <= mi2; ++i )
           {
             rgba[i] = p[i] << 4;
           }

           p += increment;
           for ( int j = 1; j < lineLength; ++j, p += increment )
           {
             for ( int i = mi1; i <= mi2; ++i )
             {
               p[i] = ( rgba[i] += ( ( p[i] << 4 ) - rgba[i] ) * mAlpha / 16 ) >> 4;
             }
           }
         }

       private:
         int mAlpha;
         LineOperationDirection mDirection;
         bool mForwardDirection;
         int mi1;
         int mi2;
     };

     static double *createGaussianKernel( int radius );

     class GaussianBlurOperation
     {
       public:
         GaussianBlurOperation( int radius, LineOperationDirection direction, QImage *destImage, double *kernel )
           : mRadius( radius )
           , mDirection( direction )
           , mDestImage( destImage )
           , mDestImageBpl( destImage->bytesPerLine() )
           , mKernel( kernel )
         {}

         typedef void result_type;

         void operator()( ImageBlock &block );

       private:
         int mRadius;
         LineOperationDirection mDirection;
         QImage *mDestImage = nullptr;
         int mDestImageBpl;
         double *mKernel = nullptr;

         inline QRgb gaussianBlurVertical( int posy, unsigned char *sourceFirstLine, int sourceBpl, int height );
         inline QRgb gaussianBlurHorizontal( int posx, unsigned char *sourceFirstLine, int width );
     };

     //flip


     class FlipLineOperation
     {
       public:
         explicit FlipLineOperation( LineOperationDirection direction )
           : mDirection( direction )
         { }

         typedef void result_type;

         LineOperationDirection direction() { return mDirection; }

         void operator()( QRgb *startRef, int lineLength, int bytesPerLine );

       private:
         LineOperationDirection mDirection;
     };


 };

 #endif // QGSIMAGEOPERATION_H

