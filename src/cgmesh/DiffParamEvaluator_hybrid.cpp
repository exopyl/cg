#include "DiffParamEvaluator.h"

//
//
//
bool MeshAlgoTensorEvaluator::ApplyHybrid (void)
{
	int nv = m_pModel->m_nVertices;
	float *v = m_pModel->m_pVertices;
	float *vn = m_pModel->m_pVertexNormals;
	Che_edge** m_edges_vertex = m_pModel->m_edges_vertex;

	int i;
	Tensor **hybrid = (Tensor**)malloc(nv*sizeof(Tensor*));
	for (i=0; i<nv; i++) hybrid[i] = new Tensor ();
	
	/* normale and principal curvatures */
	ApplyDesbrun ();
	for (i=0; i<nv; i++)
    {
		Vector3 n;
		if (!m_pDiffParams[i])
		{
			hybrid[i] = NULL;
			continue;
		}
		
		m_pDiffParams[i]->GetNormal (n);
		hybrid[i]->SetNormal (n);
		hybrid[i]->SetKappaMax (m_pDiffParams[i]->GetKappaMax ());
		hybrid[i]->SetKappaMin (m_pDiffParams[i]->GetKappaMin ());
    }
	
	/* principal directions */
	ApplySteiner ();
	for (i=0; i<nv; i++)
    {
		Vector3 tmax, tmin;
		
		if (!m_pDiffParams[i])
		{
			hybrid[i] = NULL;
			continue;
		}
		
		m_pDiffParams[i]->GetDirectionMax (tmax);
		hybrid[i]->SetDirectionMax (tmax.x, tmax.y, tmax.z);
		
		m_pDiffParams[i]->GetDirectionMin (tmin);
		hybrid[i]->SetDirectionMin (tmin.x, tmin.y, tmin.z);
    }
	
	/* save the differential parameters */
	for (i=0; i<nv; i++)
    {
		if (!m_pDiffParams[i])
		{
			hybrid[i] = NULL;
			continue;
		}
		
		Vector3 n;
		hybrid[i]->GetNormal (n);
		m_pDiffParams[i]->SetNormal (n);
		m_pDiffParams[i]->SetKappaMax (hybrid[i]->GetKappaMax ());
		m_pDiffParams[i]->SetKappaMin (hybrid[i]->GetKappaMin ());
		
		Vector3 tmax, tmin;
		
		hybrid[i]->GetDirectionMax (tmax);
		m_pDiffParams[i]->SetDirectionMax (tmax.x, tmax.y, tmax.z);
		
		hybrid[i]->GetDirectionMin (tmin);
		m_pDiffParams[i]->SetDirectionMin (tmin.x, tmin.y, tmin.z);
    }

	return true;
}
