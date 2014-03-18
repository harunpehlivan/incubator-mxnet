#ifndef MSHADOW_TENSOR_EXPR_EXT_H
#define MSHADOW_TENSOR_EXPR_EXT_H
/*!
 * \file tensor_expr_ext.h
 * \brief some extension of expressions, used to support something beyond elementwise op
 * \author Tianqi Chen
 */
#include "tensor_expr_engine-inl.hpp"
namespace mshadow{
    // Declaration of expressions goes here
    namespace expr{
        /*! 
         * \brief broadcast Tensor1D into a higher dimension Tensor
         * input: Tensor<Device,1>: ishape[0]
         * output: Tensor<Device,dimdst> : oshape[dimcast] = ishape[0]
         * \tparam Device which device it lies
         * \tparam dimdst  target tensor dimension
         * \tparam dimcast the dimension where the 1D tensor fills in by index
         */
        template<typename Device, int dimdst, int dimcast>
        struct Broadcast1DExp: public MakeTensorExp< Broadcast1DExp<Device,dimdst,dimcast>,Tensor<Device,1>,dimdst>{
            /*! \brief source operand */
            const Tensor<Device,1> &src_;
            /*! \brief construct a repmat expression from src and nrow */
            Broadcast1DExp( const Tensor<Device,1> &src, Shape<dimdst> shape ):src_(src){
                this->shape_ = shape;
            }
        };

        /*! 
         * \brief reshape the content to another shape
         * input: Tensor<Device,dimsrc>: ishape
         * output: Tensor<Device,dimdst> ishape.Size() == oshape.Size()
         *
         * \tparam Device where the data lies
         * \tparam dimdst target dimension
         * \tparam dimsrc source dimension
         */
        template<typename Device, int dimdst, int dimsrc>
        struct ReshapeExp: public MakeTensorExp< ReshapeExp<Device,dimdst,dimsrc>, Tensor<Device,dimsrc>, dimdst>{
            /*! \brief source expression */
            Tensor<Device,dimsrc> src_;
            ReshapeExp( const Tensor<Device,dimsrc> &src, Shape<dimdst> shape ):src_(src){
                utils::Assert( shape.Size() == src.shape.Size(), "reshape size must match" );
                this->shape_ = shape;
            }
        };

        /*! 
         * \brief reduction to 1 dimension tensor
         * input: Tensor<Device,k>: ishape
         * output: Tensor<Device,1> shape[0] = ishape[dimkeep];
         *
         * \tparam EType type of expression to be reduced
         * \tparam Reducer which reducer to use
         * \tparam srcdim dimension of source 
         * \tparam dimkeep which dimension to be kept, 
         */
        template<typename EType, typename Reducer,int dimkeep>
        struct ReduceTo1DExp: public Exp< ReduceTo1DExp<EType,Reducer, dimkeep>, type::kComplex >{
            /*! \brief source operand */
            EType src_;
            /*! \brief source operand, scale of the  */
            real_t scale_;
            /*! \brief construct a repmat expression from src and nrow */
            ReduceTo1DExp( EType src, real_t scale ):src_(src),scale_(scale){}
        };
        template<typename E, typename R,int d>
        inline ReduceTo1DExp<E,R,d> operator*( const ReduceTo1DExp<E,R,d> &e, real_t scale ){
            return ReduceTo1DExp<E,R,d>( e.src_, e.scale_*scale );
        }
        template<typename E, typename R,int d>
        inline ReduceTo1DExp<E,R,d> operator*( real_t scale, const ReduceTo1DExp<E,R,d> &e ){
            return ReduceTo1DExp<E,R,d>( e.src_, e.scale_*scale );
        }       
    }; // namespace expr
    
    
    // Declaration of all functions go here
    namespace expr{
        /*! 
         * \brief a expression that replicate a 1 dimension tensor in dimension dimcast
         * \param src Tensor<Device,1>: shape[0]
         * \param shape shape of output
         * \return a expresion with type Tensor<Device,dimdst>
         * \tparam dimcast target dimension where the 1D tensor will be broadcasted
         * \tparam Device which device it lies
         * \tparam dimdst dimension of destination tensor
         */
        template<int dimcast,typename Device,int dimdst>
        inline Broadcast1DExp<Device,dimdst,dimcast> broadcast( const Tensor<Device,1> &src, Shape<dimdst> shape ){
            TypeCheckPass< dimcast<dimdst >::Error_Expression_Does_Not_Meet_Dimension_Req();
            utils::Assert( src.shape[0] == shape[dimcast], "broadcast, shape mismatch" );    
            return Broadcast1DExp<Device,dimdst,dimcast>( src, shape );
        }
           
        /*! 
         * \brief a expression that reshapes a tensor to another shape
         * \param src Tensor<Device,dimsrc>: 
         * \param oshape target shape
         * \return a expresion with type Tensor<Device,dimdst> 
         * \tparam Device which device it lies
         * \tparam dimdst target dimension
         * \tparam dimsrc source dimension         
         */
        template<typename Device, int dimdst, int dimsrc>
        inline ReshapeExp<Device,dimdst,dimsrc> reshape( const Tensor<Device,dimsrc> &src, Shape<dimdst> oshape ){
            return ReshapeExp<Device,dimdst,dimsrc>( src, oshape );
        }

        /*! 
         * \brief a sum over all dimensions, except dimkeep
         * \param exp input expression that must be a matrix Tensor<?,2>
         * \return a expresion with type Tensor<Device,1> 
         * \tparam dimkeep the dimension that will be kept
         * \tparam E expression
         * \tparam etype type of expression
         */
        template<int dimkeep,  typename E, int etype>
        inline ReduceTo1DExp<E, red::sum, 0 > sumall_except_dim( const Exp<E,etype> &exp ){
            return ReduceTo1DExp<E,red::sum,dimkeep>( exp.self(), 1.0f );
        }

        // short cut functions 
        /*! 
         * \brief a expression that replicate a 1 dimension tensor for nrow times 
         * \param src Tensor<Device,1>: shape[0]
         * \param nrow number of rows to replicate
         * \return a expresion with type Tensor<Device,2> shape[0], shape[1] = nrow
         * \tparam Device which device it lies
         */
        template<typename Device>
        inline Broadcast1DExp<Device,2,0> repmat( const Tensor<Device,1> &src, index_t nrow ){
            return broadcast<0>( src, Shape2( nrow, src.shape[0] ) );
        }
        /*! 
         * \brief a expression that sum over rows of a matrix
         * \param exp input expression that must be a matrix Tensor<?,2>
         * \return a expresion with type Tensor<Device,1> 
         * \tparam E expression
         * \tparam etype type of expression
         */
        template<typename E, int etype>
        inline ReduceTo1DExp<E, red::sum, 0 > sum_rows( const Exp<E,etype> &exp ){
            return sumall_except_dim<0>( exp );
        }
    }; // namespace expr
}; // namespace mshadow

// ==================================================
//  implementations afterwards, 
//  no need to read if only use the functions
// --------------------------------------------------
namespace mshadow{
    namespace expr{
        template<typename SV, typename Device, typename EType, typename Reducer>
        struct ExpComplexEngine< SV, Device, 1, ReduceTo1DExp<EType,Reducer,0> >{
            inline static void Eval( Tensor<Device,1> &dst, const ReduceTo1DExp<EType,Reducer,0> &exp ){                
                MapReduceKeepLowest<SV,Reducer>( dst, exp.src_, exp.scale_ );
            }
        };
    }; // namespace expr

    namespace expr{
        /*! \brief execution plan of Broadcast1DExp */
        template<typename Device, int dimdst, int dimcast>
        struct Plan< Broadcast1DExp<Device,dimdst,dimcast> >{
        public:
            Plan( const Broadcast1DExp<Device,dimdst,dimcast> &e ): dptr_( e.src_.dptr ){
                TypeCheckPass< dimcast!=0 >::Error_Expression_Does_Not_Meet_Dimension_Req();
                ystride_ = 1;                
                #pragma unroll
                for( int i = 1; i < dimcast; ++ i ){
                    ystride_ *= e.shape_[i];
                }
                length_ = e.shape_[ dimcast ];
            }
            MSHADOW_XINLINE real_t Eval( index_t y, index_t x ) const{                
                return dptr_[ (y / ystride_) % length_ ];
            }
        private:
            const real_t  *dptr_;
            index_t  ystride_, length_;
        };

        /*! \brief execution plan of Broadcast1DExp */
        template<typename Device, int dimdst>
        struct Plan< Broadcast1DExp<Device,dimdst,0> >{
        public:
            Plan( const Broadcast1DExp<Device,dimdst,0> &e ): dptr_( e.src_.dptr ){}
            MSHADOW_XINLINE real_t Eval( index_t y, index_t x ) const{
                return dptr_[ x ];
            }
        private:
            const real_t *dptr_;
        };
    }; // namespace expr

    namespace expr{
        /*! \brief execution plan of repmat */
        template<typename Device, int dimsrc, int dimdst>
        struct Plan< ReshapeExp<Device,dimsrc,dimdst> >{
        public:
            Plan( const ReshapeExp<Device,dimsrc,dimdst> &e ): dptr_( e.src_.dptr ){
                oshape0_ = e.shape_[0];
                ishape0_ = e.src_.shape[0];
                istride_ = e.src_.shape.stride_;
            }
            MSHADOW_XINLINE real_t Eval( index_t y, index_t x ) const{
                const index_t idx = y * oshape0_ + x;
                return dptr_[ ( idx / ishape0_ ) * istride_ + ( idx % ishape0_ ) ]; 
            }
        private:
            const real_t *dptr_;
            index_t oshape0_, ishape0_, istride_;
        };        
    }; // namespace expr
}; // namespace mshadow

#if MSHADOW_USE_SSE 
// implementations of SSE support, if possible
#include "tensor_sse-inl.hpp"
namespace mshadow{
    namespace expr{
        template<int dimdst>
        struct SSECheck< Broadcast1DExp<cpu,dimdst,0> >{
            const static bool kPass = true;
        };
        template<int dimdst>
        struct SSEAlignCheck<2, Broadcast1DExp<cpu,dimdst,0> >{
            inline static bool Check( const Broadcast1DExp<cpu,dimdst,0> &exp ){
                return sse2::CheckAlign( exp.src_.dptr );
            }
        };
        template<int dimdst>
        class SSEPlan< Broadcast1DExp<cpu,dimdst,0> >{
        public:
            SSEPlan( const Broadcast1DExp<cpu,dimdst,0> &t )
                :dptr_(t.src_.dptr){}
            MSHADOW_CINLINE sse2::FVec<real_t> EvalSSE( index_t y, index_t x ) const{
                return sse2::FVec<real_t>( &dptr_[ x ] );
            }
            MSHADOW_CINLINE real_t Eval( index_t y, index_t x ) const{
                return dptr_[ x ];
            }
        private:
            const real_t  *dptr_;
        };
    };
};
#endif

#endif

