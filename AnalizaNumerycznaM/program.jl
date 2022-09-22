using Plots
using Printf

using Polynomials
using Plotly

#----------------------------------------------------
# COMPUTING COEFFICIENTS sigma_i (IN LAGRANGIAN FORM)
# given : COEFFICIENTS a_i (IN NEWTON FORM)
#----------------------------------------------------

function compute_coeff_sigma( X::Vector{Float64}, A::Vector{Float64} )
    n = length(X) - 1
    a = Array{Float64,2}(undef, n+5, n+5)

    for i in 1:n+1  
        value = getindex(A,i)
        setindex!(a, value, i, 1)
    end

    for i in 2:n+1   
        for k in 1:i-1
        value = getindex(a,k,i-1)/(getindex(X,k)-getindex(X,i))
        setindex!(a,value,k,i)

        value = getindex(a,i,k) - getindex(a,k,i)
        setindex!(a,value,i,k+1)
        end
    end

    sigma = Array{Float64,1}(undef,n+5)
    for i in 1:n+1
        setindex!(sigma,getindex(a,i,n+1),i)
    end
    return sigma
end

function test_compute_coeff_sigma( )
    n = 10
    a = -1
    b = 1
    
    nodes = Array{Float64,1}(undef,n+1)
    
    h = (b-a)/n
    for i in 0:n
        setindex!(nodes,a +i*h, i+1)
    end

    a_i = Array{Float64,1}(undef,n+1)
    for i in 0:n
        setindex!(a_i,i*0.1, i+1)
    end

    sigma = compute_coeff_sigma(nodes,a_i)
    @printf("coeff sigma :\n")
    for i in 1:n+1
        @printf("%.16f ", getindex(sigma,i))
    end
    @printf("\n")
end

test_compute_coeff_sigma()

#----------------------------------------------------
# COMPUTING COEFFICIENTS wi (IN LAGRANGIAN FORM)
# given : X set of x_i
#----------------------------------------------------

function compute_coeff_wi( X::Vector{Float64} )
    n = length(X) - 1
    a = Array{Float64,2}(undef, n+1, n+1)

    for i in 1:n+1 
        setindex!(a,0,i)
    end
    setindex(a,1,1)

    return compute_coeff_sigma(X,a)
end

#----------------------------------------------------
# COMPUTING COEFFICIENTS sigma_i (IN LAGRANGIAN FORM)
# given : X set of x_i, and function f 
#----------------------------------------------------

function compute_coeff_sigma_from_nodes( X::Vector{Float64}, f )
    w_i = compute_coeff_wi(X)
    sigma = Array{Float64,1}(undef,n+1)

    for i in 1:n+1 
        value = getindex(w_i,i) * f(getindex(X,i))
        setindex!(sigma,value,i)
    end
    return sigma
end

#----------------------------------------------------
# COMPUTING COEFFICIENTS ai (IN NEWTON FORM)
# given : COEFFICIENTS sigma_i (IN LAGRANGIAN FORM)
#----------------------------------------------------

function compute_coeff_ai( X::Vector{Float64}, Sigma::Vector{Float64} )
    n = length(X) - 1
    s = Array{Float64,2}(undef, n+1, n+1)

    for i in 1:n+1
        setindex!(s,getindex(Sigma,i),i,0)
    end

    for k in 1:n+1
        sum = 0
        for i in 1:(n-k)          
            sum+=getindex(s,i,k)
        end
        setindex!(s,sum,n-k,k+1)  
        for i in (n+2-k):-1:1
            value = (getindex(X,i) - getindex(X,n-k)) * getindex(s,i,k)
            setindex!(s,value,i,k+1)
        end
    end

    a = Array{Float64,2}(undef,1)
    for i in 1:n+1 
        setindex!(a,getindex(s,i,n+1 -i),i)                
    end

    return a 
end

#----------------------------------------------------
# COMPUTING LAGRANGIAN POLYNOMIAL VALUE IN x
# given : X set of x_i, and Y set of f(x_i)
#----------------------------------------------------

function compute_lagrangian_pol_value( X::Vector{Float64}, Y::Vector{Float64}, x )
    coeff_wi = compute_coeff_wi(X)

    n = length(X) -1
    l = 0.0 
    m = 0.0

    for i in 1:n+1
        t = x - getindex(X,i)
        w = getindex(coeff_wi,i) / t

        l = l + w*getindex(Y,i)
        m = m + w
    end 
    
    result = l/m
    return result
end


#----------------------------------------------------
# COMPUTING NEWTON POLYNOMIAL VALUE IN x
# given : X set of x_i, and Y set of f(x_i)
#----------------------------------------------------

function compute_newton_pol_value( X::Vector{Float64}, Y::Vector{Float64}, x )
    coeff_wi = compute_coeff_wi(X)
    
    n =length(X) -1
    coeff_sigma = Array{Float64,1}(undef,n+5)
    for i in 1:n+1 
        value =getindex(coeff_sigma,i)*getindex(Y,i)
        setindex!(coeff_sigma,value,i)
    end

    coeff_ai = compute_coeff_ai(X,coeff_sigma)

    w_1 = getindex(coeff_ai,n+1)    
    w_2 = 0 
    for i in n:-1:1
        temp = w_2

        w_2 = w_1 * (x - getindex(X,k)) + getindex(coeff_ai,i)
        w_1 = temp
    end

    return w_2
end
